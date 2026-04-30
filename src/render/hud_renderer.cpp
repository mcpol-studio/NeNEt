#include "hud_renderer.h"

#include "buffer.h"
#include "../ui/font.h"

#include <spdlog/spdlog.h>

namespace nenet {

HudRenderer::HudRenderer(VmaAllocator allocator, size_t maxQuads)
    : allocator_(allocator), maxQuads_(maxQuads) {
    vertexBuffer_ = std::make_unique<Buffer>(
        allocator_, maxQuads_ * 4 * sizeof(HudVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    indexBuffer_ = std::make_unique<Buffer>(
        allocator_, maxQuads_ * 6 * sizeof(uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT);

    vertices_.reserve(maxQuads_ * 4);
    indices_.reserve(maxQuads_ * 6);
    spdlog::info("HudRenderer 已初始化 maxQuads={}", maxQuads_);
}

HudRenderer::~HudRenderer() = default;

void HudRenderer::clear() {
    vertices_.clear();
    indices_.clear();
    indexCount_ = 0;
}

void HudRenderer::addText(const std::string& s, float ndcX, float ndcY,
                           float pixelSize, const glm::vec3& color) {
    float cursorX = ndcX;
    for (char c : s) {
        const uint8_t* bm = getCharBitmap(c);
        for (int row = 0; row < kFontHeight; ++row) {
            const uint8_t bits = bm[row];
            for (int col = 0; col < kFontWidth; ++col) {
                if (bits & (1u << (kFontWidth - 1 - col))) {
                    const float x0 = cursorX + col * pixelSize;
                    const float y0 = ndcY + row * pixelSize;
                    const float x1 = x0 + pixelSize;
                    const float y1 = y0 + pixelSize;
                    const uint32_t base = static_cast<uint32_t>(vertices_.size());

                    if (vertices_.size() + 4 > maxQuads_ * 4) return;

                    vertices_.push_back({{x0, y0}, color});
                    vertices_.push_back({{x0, y1}, color});
                    vertices_.push_back({{x1, y1}, color});
                    vertices_.push_back({{x1, y0}, color});
                    indices_.push_back(base + 0);
                    indices_.push_back(base + 1);
                    indices_.push_back(base + 2);
                    indices_.push_back(base + 0);
                    indices_.push_back(base + 2);
                    indices_.push_back(base + 3);
                }
            }
        }
        cursorX += kFontAdvance * pixelSize;
    }
}

void HudRenderer::addRect(float x0, float y0, float x1, float y1, const glm::vec3& color) {
    if (vertices_.size() + 4 > maxQuads_ * 4) return;
    const uint32_t base = static_cast<uint32_t>(vertices_.size());
    vertices_.push_back({{x0, y0}, color});
    vertices_.push_back({{x0, y1}, color});
    vertices_.push_back({{x1, y1}, color});
    vertices_.push_back({{x1, y0}, color});
    indices_.push_back(base + 0);
    indices_.push_back(base + 1);
    indices_.push_back(base + 2);
    indices_.push_back(base + 0);
    indices_.push_back(base + 2);
    indices_.push_back(base + 3);
}

void HudRenderer::upload() {
    if (vertices_.empty() || indices_.empty()) {
        indexCount_ = 0;
        return;
    }
    vertexBuffer_->upload(vertices_.data(), vertices_.size() * sizeof(HudVertex));
    indexBuffer_->upload(indices_.data(), indices_.size() * sizeof(uint32_t));
    indexCount_ = static_cast<uint32_t>(indices_.size());
}

void HudRenderer::draw(VkCommandBuffer cmd) const {
    if (indexCount_ == 0) return;
    VkBuffer vbufs[] = {vertexBuffer_->handle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer_->handle(), 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

}
