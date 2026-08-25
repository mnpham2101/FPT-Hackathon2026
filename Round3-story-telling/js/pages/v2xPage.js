import { createContentPage } from "./contentPage.js";

export function createV2xPage() {
  return createContentPage({
    contentPath: "content/v2x.md",
    accent: 0x7fe8ff,
  });
}
