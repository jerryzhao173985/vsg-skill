#!/usr/bin/env python3
"""
eval-session.py — deterministic, attributed, evidence-anchored evaluator for the
`vsg` skill. Reads a /vsg session transcript (.jsonl) and emits mechanical signals
per evaluation dimension. NO LLM judgment in the score — only counts/regex/exit
codes, so the same transcript always yields the same record (stable by construction).

Dimensions (orthogonal):
  D2 COVERAGE        — Reads to files OUTSIDE the skill (the skill didn't carry it)
  D3 SELF-CONTAINED  — Read a file the skill *cited* but didn't teach (pointer-not-coverage)
  D4 IDIOM           — anti-idioms in produced code (new-not-create, missing compile, etc.)
  D5 HONESTY         — hallucinated vsg:: symbols (not in headers) vs correct [VERIFY]/scope-out
  (D1 GROUNDING TRUTH = the compile+link+cite probes, run separately; global not per-session.)

Each finding is attributed: SKILL-GAP | SKILL-BUG | AGENT | ENV. Only SKILL-* count
against the skill. Run: eval-session.py <session.jsonl> <skill_dir> <vsg_repo> <examples_repo>
"""
import json, os, re, sys

GAP_RE = re.compile(r"(references? (?:don'?t|do not|doesn'?t) (?:fully )?cover|not (?:fully )?cover(?:ed)?|"
                    r"isn'?t covered|skill (?:don'?t|doesn'?t) cover|the (?:references?|skill) (?:lack|miss)|"
                    r"had to (?:read|ground|reconstruct|discover)|beyond the skill|not in the (?:skill|references?))", re.I)
HONEST_RE = re.compile(r"(\[VERIFY\]|out of scope|out-of-scope|does not exist|doesn'?t exist|not part of (?:public )?vsg|"
                       r"no such (?:api|method|class)|cannot ground|not a public)", re.I)
VSG_SYM = re.compile(r"\bvsg::([A-Z][A-Za-z0-9_]+)")

def load(jl):
    """Return (events, toolout) where events is ordered [(role, item)] and
    toolout maps tool_use_id -> concatenated output text."""
    events, toolout = [], {}
    for ln in open(jl, errors="ignore"):
        try: o = json.loads(ln)
        except: continue
        # top-level tool results
        tur = o.get("toolUseResult"); tid = o.get("toolUseID")
        if tid and tur is not None:
            toolout[tid] = toolout.get(tid, "") + _txt(tur)
        m = o.get("message", o)
        role = m.get("role") if isinstance(m, dict) else None
        c = m.get("content") if isinstance(m, dict) else None
        if isinstance(c, str):
            events.append((role, {"type": "text", "text": c}))
        elif isinstance(c, list):
            for x in c:
                if not isinstance(x, dict): continue
                events.append((role, x))
                if x.get("type") == "tool_result":
                    rid = x.get("tool_use_id")
                    if rid: toolout[rid] = toolout.get(rid, "") + _txt(x.get("content"))
    return events, toolout

def _txt(v):
    if isinstance(v, str): return v
    if isinstance(v, list): return " ".join(_txt(i) for i in v)
    if isinstance(v, dict): return _txt(v.get("text") or v.get("content") or v.get("stdout") or "")
    return str(v) if v is not None else ""

def sentences(t):
    return [s.strip() for s in re.split(r"(?<=[.!?\n])\s", t) if s.strip()]

def main():
    jl, skill, vsg, ex = sys.argv[1:5]
    events, toolout = load(jl)
    skill_abs = os.path.abspath(skill)
    # index skill cites: which non-skill paths does the skill point at? (for D3)
    skill_cited = set()
    for dp,_,fs in os.walk(skill_abs):
        for f in fs:
            if f.endswith(".md"):
                for m in re.finditer(r"((?:include/vsg|src/vsg|examples)/[A-Za-z0-9_./]+\.(?:h|cpp|hpp))", open(os.path.join(dp,f),errors="ignore").read()):
                    skill_cited.add(m.group(1))
    # D5 hallucination test is grep-against-headers (below), NOT an index — an index
    # of header-basenames misses real structs/typedefs (GeometryInfo, QueueSettings,
    # MappedData, PI) and produces false positives. The replay build is the ultimate
    # authority: code that compiles+links has zero hallucinated symbols by definition.
    import subprocess
    def symbol_absent(sym):
        return subprocess.run(["grep","-rqwE", re.escape(sym), os.path.join(vsg,"include")]).returncode != 0

    prompt = ""
    skill_reads, out_reads, writes = [], [], []
    gap_hits, honest_hits = [], []
    last_text = ""
    for role, x in events:
        t = x.get("type")
        if t == "text":
            txt = x.get("text","") or ""
            last_text = txt
            if role == "user" and txt.strip().startswith("/vsg") and not prompt:
                prompt = txt.strip().split("\n")[0][:140]
            if role == "assistant":
                for s in sentences(txt):
                    if GAP_RE.search(s): gap_hits.append(s[:200])
                    if HONEST_RE.search(s): honest_hits.append(s[:160])
        elif t == "tool_use":
            nm = x.get("name"); inp = x.get("input",{}) or {}
            if nm == "Read":
                fp = inp.get("file_path","")
                if skill_abs in os.path.abspath(fp): skill_reads.append(fp.split("/skills/vsg/")[-1])
                else:
                    rel = fp.split("VulkanSceneGraph/")[-1].split("vsgExamples/")[-1]
                    if re.search(r"(include/vsg|src/vsg|examples)/", rel):
                        out_reads.append({"path": rel, "pointer": rel in skill_cited, "why": last_text[-160:].replace("\n"," ")})
            elif nm == "Bash":
                cmd = inp.get("command","") or ""
                # `cat <header/example>` is an improper full read (no line numbers) — count it like a Read.
                # grep/sed/head SEARCH a file and are fine (not counted), per the skill's Read-not-cat guidance.
                for cm in re.finditer(r'\bcat\s+(?:-\S+\s+)*([^\s|;&)<>]+)', cmd):
                    p = cm.group(1); rel = re.sub(r'.*(?:VulkanSceneGraph|vsgExamples)/','',p)
                    if '/skills/vsg/' not in p and re.search(r'(include/vsg|src/vsg|examples)/.*\.(h|cpp|hpp)$', rel):
                        out_reads.append({"path": rel+" (cat)", "pointer": rel in skill_cited, "why": "Bash cat — should use Read"})
            elif nm in ("Write","Edit"):
                fp = inp.get("file_path","")
                if fp.endswith((".cpp",".h",".hpp",".cxx")):
                    writes.append({"path": fp, "content": inp.get("content","") or inp.get("new_string","")})

    # D4 idiom checks on produced C++
    idioms = []
    code = "\n".join(w["content"] for w in writes if w["content"])
    if code:
        if re.search(r"\bnew\s+vsg::", code): idioms.append(("new-not-create","SKILL-BUG?","`new vsg::` in produced code"))
        loop = "advanceToNextFrame" in code
        if loop and "->compile()" not in code and "compile(" not in code:
            idioms.append(("missing-compile","AGENT?","frame loop present but no compile() call"))
        if "read_cast" in code and not re.search(r"read_cast[^;]*\)[\s\S]{0,80}?if\s*\(\s*!?", code):
            idioms.append(("unchecked-read_cast","minor","read_cast without an adjacent null-check"))
        if "Window::create" in code and not re.search(r"Window::create[^;]*\)[\s\S]{0,80}?if\s*\(\s*!?", code):
            idioms.append(("unchecked-window","minor","Window::create without an adjacent null-check"))
        # lighting: lit Builder geometry but no light anywhere
        lit_builder = re.search(r"create(Box|Sphere|Cylinder|Cone|Capsule|Disk|Quad|HeightField)\(", code) or "StateInfo" in code
        has_light = re.search(r"createHeadlight|(Ambient|Directional|Point|Spot)Light::create", code)
        used_convenience = "createCommandGraphForView" in code  # adds headlight by default
        if lit_builder and not has_light and not used_convenience:
            idioms.append(("lit-no-light","SKILL-GAP?","Builder/lit geometry, hand-wired, no light → black-geometry risk"))
        create_n = len(re.findall(r"::create\(", code)); new_n = len(re.findall(r"\bnew\s+vsg::", code))
    else:
        create_n = new_n = 0

    # D5 hallucination: produced vsg:: symbol that grep-resolves NOWHERE in headers.
    # (Authoritative answer = the replay build; this is the static cross-check.)
    used_syms = set(VSG_SYM.findall(code)) if code else set()
    halluc = sorted(s for s in used_syms if s[:1].isupper() and symbol_absent(s))
    # build results (D1 local): scan bash outputs for compile/link verdicts
    builds = []
    for role,x in events:
        if x.get("type")=="tool_use" and x.get("name")=="Bash":
            cmd = (x.get("input",{}) or {}).get("command","")
            if re.search(r"cmake|clang\+\+|make\b", cmd):
                out = toolout.get(x.get("id",""), "")
                verdict = ("BUILT" if re.search(r"Built target|Build complete|\] Linking", out) else
                           "ERROR" if re.search(r"error:|undefined symbol|FAILED|No such", out) else "?")
                builds.append(verdict)

    rep = {
        "session": os.path.basename(jl), "prompt": prompt,
        "skill_files_loaded": sorted(set(skill_reads)),
        "D2_coverage": {"out_reads": len([r for r in out_reads if not r['pointer']]),
                        "evidence": [r for r in out_reads if not r['pointer']][:8]},
        "D3_self_contained": {"pointer_reads": len([r for r in out_reads if r['pointer']]),
                              "evidence": [r['path'] for r in out_reads if r['pointer']][:8]},
        "gap_statements": gap_hits[:6],
        "D4_idiom": {"create_calls": create_n, "new_calls": new_n, "violations": idioms},
        "D5_honesty": {"hallucinated_symbols": halluc, "honesty_markers": len(honest_hits),
                       "honesty_evidence": honest_hits[:4]},
        "builds": builds,
        "files_produced": [w["path"].split("vsgExamples/")[-1] for w in writes],
    }
    print(json.dumps(rep, indent=2))

if __name__ == "__main__":
    main()
