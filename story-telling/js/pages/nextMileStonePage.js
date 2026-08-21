import { createContentPage } from "./contentPage.js";

export function createNextMileStonePage() {
  return createContentPage({
    contentPath: "content/the-road-ahead.md",
    accent: 0x5be37a,
  });
}
