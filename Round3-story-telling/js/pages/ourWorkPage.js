import { createContentPage } from "./contentPage.js";

export function createOurWorkPage() {
  return createContentPage({
    contentPath: "content/our-work-process.md",
    accent: 0x8b7bff,
  });
}
