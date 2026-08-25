import { createContentPage } from "./contentPage.js";

export function createDemoPage() {
  return createContentPage({
    contentPath: "content/demo.md",
    accent: 0x2ec4b6,
  });
}
