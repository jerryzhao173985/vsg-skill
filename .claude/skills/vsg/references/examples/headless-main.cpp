// COMPILE+LINK-VERIFIED against installed VulkanSceneGraph 1.1.14 via find_package(vsg).
// Distilled from examples/app/vsgheadless/vsgheadless.cpp; copy this whole file as a headless starter.
// Headless / offscreen VSG renderer — no window, no swapchain, no present().
//
// Renders a model into a Framebuffer we allocate ourselves, then copies the
// color image into host-visible memory and writes it to disk. Every step is
// grounded in examples/app/vsgheadless/vsgheadless.cpp (cited inline as gh:<line>).
//
// ⚠ macOS / MoltenVK: this surface-less path SEGFAULTS at first-frame record
//   (BindGraphicsPipeline::record hits a null per-view pipeline impl). The
//   official vsgheadless reproduces the identical crash on this host. On macOS,
//   render to a window-backed swapchain and capture from it instead — see
//   examples/app/vsgscreenshot/vsgscreenshot.cpp. Native Vulkan (Linux/Windows)
//   is unaffected.

#include <vsg/all.h>          // barrel include — the whole public surface (vsg/all.h)

#ifdef vsgXchange_FOUND
#    include <vsgXchange/all.h>
#endif

#include <cstring>
#include <iostream>
#include <tuple>

// ── Offscreen render pass: like the default, but leaves the color attachment in
//    TRANSFER_SRC_OPTIMAL so we can copy from it. (gh:29-81)
static vsg::ref_ptr<vsg::RenderPass> createOffscreenRenderPass(vsg::Device* device, VkFormat imageFormat, VkFormat depthFormat)
{
    auto colorAttachment = vsg::defaultColorAttachment(imageFormat);
    auto depthAttachment = vsg::defaultDepthAttachment(depthFormat);
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL; // the offscreen difference (gh:34)
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    vsg::RenderPass::Attachments attachments{colorAttachment, depthAttachment};

    vsg::AttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(colorRef);
    subpass.depthStencilAttachments.emplace_back(depthRef);
    vsg::RenderPass::Subpasses subpasses{subpass};

    vsg::SubpassDependency colorDep = {};
    colorDep.srcSubpass = VK_SUBPASS_EXTERNAL;
    colorDep.dstSubpass = 0;
    colorDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vsg::SubpassDependency depthDep = {};
    depthDep.srcSubpass = VK_SUBPASS_EXTERNAL;
    depthDep.dstSubpass = 0;
    depthDep.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDep.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depthDep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthDep.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    vsg::RenderPass::Dependencies dependencies{colorDep, depthDep};
    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}

// ── Color attachment image+view: COLOR_ATTACHMENT | TRANSFER_SRC so it is both a
//    render target and a copy source. (gh:198-214)
static vsg::ref_ptr<vsg::ImageView> createColorImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat imageFormat)
{
    auto colorImage = vsg::Image::create();
    colorImage->imageType = VK_IMAGE_TYPE_2D;
    colorImage->format = imageFormat;
    colorImage->extent = VkExtent3D{extent.width, extent.height, 1};
    colorImage->mipLevels = 1;
    colorImage->arrayLayers = 1;
    colorImage->samples = VK_SAMPLE_COUNT_1_BIT;
    colorImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    colorImage->usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    colorImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return vsg::createImageView(device, colorImage, VK_IMAGE_ASPECT_COLOR_BIT); // (include/vsg/state/ImageView.h:68)
}

// ── Depth attachment image+view. (gh:216-232)
static vsg::ref_ptr<vsg::ImageView> createDepthImageView(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, VkFormat depthFormat)
{
    auto depthImage = vsg::Image::create();
    depthImage->imageType = VK_IMAGE_TYPE_2D;
    depthImage->extent = VkExtent3D{extent.width, extent.height, 1};
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->samples = VK_SAMPLE_COUNT_1_BIT;
    depthImage->format = depthFormat;
    depthImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    depthImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return vsg::createImageView(device, depthImage, vsg::computeAspectFlagsForFormat(depthFormat));
}

// ── Color capture: barrier → blit/copy → barrier. Returns the Commands subgraph
//    (added to the CommandGraph so it runs in the same submission, after the
//    render) and the host-visible destination Image we read pixels from. (gh:234-392)
static std::pair<vsg::ref_ptr<vsg::Commands>, vsg::ref_ptr<vsg::Image>>
createColorCapture(vsg::ref_ptr<vsg::Device> device, const VkExtent2D& extent, vsg::ref_ptr<vsg::Image> sourceImage, VkFormat sourceImageFormat)
{
    auto physicalDevice = device->getPhysicalDevice();
    VkFormat targetImageFormat = sourceImageFormat;

    // Blit can convert format on the fly; copy cannot. Pick based on support. (gh:244-259)
    VkFormatProperties srcProps, dstProps;
    vkGetPhysicalDeviceFormatProperties(*physicalDevice, sourceImageFormat, &srcProps);
    vkGetPhysicalDeviceFormatProperties(*physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &dstProps);
    bool supportsBlit = (srcProps.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) &&
                        (dstProps.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);
    if (supportsBlit) targetImageFormat = VK_FORMAT_R8G8B8A8_UNORM;

    // Host-visible, linear-tiled destination image we can map after the copy. (gh:266-283)
    auto destinationImage = vsg::Image::create();
    destinationImage->imageType = VK_IMAGE_TYPE_2D;
    destinationImage->format = targetImageFormat;
    destinationImage->extent = VkExtent3D{extent.width, extent.height, 1};
    destinationImage->arrayLayers = 1;
    destinationImage->mipLevels = 1;
    destinationImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    destinationImage->samples = VK_SAMPLE_COUNT_1_BIT;
    destinationImage->tiling = VK_IMAGE_TILING_LINEAR;
    destinationImage->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    destinationImage->compile(device);
    auto deviceMemory = vsg::DeviceMemory::create(device, destinationImage->getMemoryRequirements(device->deviceID),
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    destinationImage->bind(deviceMemory, 0);

    auto commands = vsg::Commands::create();

    // 1) dest UNDEFINED→TRANSFER_DST, source (already TRANSFER_SRC from render pass) confirmed. (gh:291-322)
    auto toTransferDst = vsg::ImageMemoryBarrier::create(
        0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, destinationImage,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    auto srcToTransferSrc = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, sourceImage,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    commands->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, toTransferDst, srcToTransferSrc));

    // 2) blit (with format conversion) or plain copy. (gh:324-368)
    if (supportsBlit)
    {
        VkImageBlit region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[1] = VkOffset3D{static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1};
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[1] = VkOffset3D{static_cast<int32_t>(extent.width), static_cast<int32_t>(extent.height), 1};

        auto blit = vsg::BlitImage::create();
        blit->srcImage = sourceImage;
        blit->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blit->dstImage = destinationImage;
        blit->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blit->regions.push_back(region);
        blit->filter = VK_FILTER_NEAREST;
        commands->addChild(blit);
    }
    else
    {
        VkImageCopy region{};
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.extent = VkExtent3D{extent.width, extent.height, 1};

        auto copy = vsg::CopyImage::create();
        copy->srcImage = sourceImage;
        copy->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copy->dstImage = destinationImage;
        copy->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy->regions.push_back(region);
        commands->addChild(copy);
    }

    // 3) dest TRANSFER_DST→GENERAL so host memory can be mapped. (gh:371-389)
    auto toGeneral = vsg::ImageMemoryBarrier::create(
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, destinationImage,
        VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
    commands->addChild(vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, toGeneral));

    return {commands, destinationImage};
}

int main(int argc, char** argv)
{
    VkExtent2D extent{2048, 1024};
    VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    vsg::CommandLine arguments(&argc, argv);
    auto numFrames = arguments.value(1, "-f");
    auto colorFilename = arguments.value<vsg::Path>("screenshot.vsgb", {"--color-file", "--cf"});
    arguments.read({"--extent", "-w"}, extent.width, extent.height);
    if (arguments.errors()) return arguments.writeErrorMessages(std::cerr);

    if (argc <= 1)
    {
        std::cout << "Usage: myvsgheadless <model.vsgt> [-w W H] [-f N] [--cf out.vsgb]\n";
        return 1;
    }

    auto options = vsg::Options::create();
#ifdef vsgXchange_FOUND
    options->add(vsgXchange::all::create()); // enable 3rd-party model/image loaders
#endif

    auto vsg_scene = vsg::read_cast<vsg::Node>(argv[1], options); // nullable — check it
    if (!vsg_scene)
    {
        std::cout << "Failed to load: " << argv[1] << std::endl;
        return 1;
    }

    // ── Surface-less device: Instance → graphics PhysicalDevice (no Surface) → Device.
    //    No window/swapchain extensions are enabled. (gh:561-576)
    auto instance = vsg::Instance::create(vsg::Names{}, vsg::Names{}, VK_API_VERSION_1_0);
    auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    if (!physicalDevice || queueFamily < 0)
    {
        std::cout << "Could not find a graphics-capable PhysicalDevice." << std::endl;
        return 1;
    }
    vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};
    auto deviceFeatures = vsg::DeviceFeatures::create();
    auto device = vsg::Device::create(physicalDevice, queueSettings, vsg::Names{}, vsg::Names{}, deviceFeatures);

    // ── Frame the scene from its bounds (no window extent2D() to lean on). (gh:578-598)
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 1.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective = vsg::Perspective::create(30.0,
        static_cast<double>(extent.width) / static_cast<double>(extent.height), 0.001 * radius, radius * 4.5);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(extent));

    // ── Our own offscreen target: color + depth attachments → render pass → framebuffer. (gh:609-616)
    auto colorImageView = createColorImageView(device, extent, imageFormat);
    auto depthImageView = createDepthImageView(device, extent, depthFormat);
    auto renderPass = createOffscreenRenderPass(device, imageFormat, depthFormat);
    auto framebuffer = vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView, depthImageView},
                                                extent.width, extent.height, 1);

    // ── Capture subgraph: copies the rendered color image into host-visible memory. (gh:628)
    vsg::ref_ptr<vsg::Commands> colorCapture;
    vsg::ref_ptr<vsg::Image> copiedColor;
    std::tie(colorCapture, copiedColor) = createColorCapture(device, extent, colorImageView->image, imageFormat);

    // ── RenderGraph bound to OUR framebuffer (not a window). (gh:651-676)
    auto renderGraph = vsg::RenderGraph::create();
    renderGraph->framebuffer = framebuffer;
    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = extent;
    renderGraph->setClearValues({{0.1f, 0.1f, 0.15f, 1.0f}}, VkClearDepthStencilValue{0.0f, 0});

    auto view = vsg::View::create(camera, vsg_scene);
    view->addChild(vsg::createHeadlight()); // hand-wired View → must add a light or lit geometry renders black
    renderGraph->addChild(view);

    // ── Surface-less CommandGraph(device, queueFamily): render then capture, same submission. (gh:678-682)
    auto commandGraph = vsg::CommandGraph::create(device, queueFamily);
    commandGraph->addChild(renderGraph);
    commandGraph->addChild(colorCapture);

    auto viewer = vsg::Viewer::create();
    // No present-capable family on this command graph → viewer makes a
    // RecordAndSubmitTask but NO Presentation. (gh:694)
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    viewer->compile(); // once, before the loop — builds pipelines/descriptors/buffers

    uint64_t waitTimeout = 1999999999; // ~1s in ns

    // ── Render loop with NO present(): advance → handleEvents → update → recordAndSubmit. (gh:701-784)
    while (viewer->advanceToNextFrame() && (numFrames--) > 0)
    {
        std::cout << "Frame " << viewer->getFrameStamp()->frameCount << std::endl;
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        // (intentionally no viewer->present(); there is no swapchain to present to)
    }

    // ── Read pixels back: wait on the fence, query row pitch, map, write. (gh:789-818)
    viewer->waitForFences(0, waitTimeout);

    VkImageSubresource subResource{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0};
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(*device, copiedColor->vk(device->deviceID), &subResource, &subResourceLayout);
    auto deviceMemory = copiedColor->getDeviceMemory(device->deviceID);

    size_t destRowWidth = extent.width * sizeof(vsg::ubvec4);
    vsg::ref_ptr<vsg::Data> imageData;
    if (destRowWidth == subResourceLayout.rowPitch)
    {
        // Tight rows: map straight into a 2D array (auto-unmaps on destruction).
        imageData = vsg::MappedData<vsg::ubvec4Array2D>::create(
            deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{imageFormat}, extent.width, extent.height);
    }
    else
    {
        // Padded rows: map flat, then copy row-by-row into a tight 2D array.
        auto mapped = vsg::MappedData<vsg::ubyteArray>::create(
            deviceMemory, subResourceLayout.offset, 0, vsg::Data::Properties{imageFormat},
            subResourceLayout.rowPitch * extent.height);
        auto dst = vsg::ubvec4Array2D::create(extent.width, extent.height, vsg::Data::Properties{imageFormat});
        for (uint32_t row = 0; row < extent.height; ++row)
            std::memcpy(dst->dataPointer(row * extent.width),
                        mapped->dataPointer(row * subResourceLayout.rowPitch), destRowWidth);
        imageData = dst;
    }

    if (vsg::write(imageData, colorFilename))
        std::cout << "Wrote " << colorFilename << " (" << extent.width << "x" << extent.height << ")\n";
    else
        std::cout << "Failed to write " << colorFilename << std::endl;

    return 0; // ref_ptr cleans everything up
}
