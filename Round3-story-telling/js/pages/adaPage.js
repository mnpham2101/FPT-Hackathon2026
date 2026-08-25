import { createContentPage } from "./contentPage.js";

export function createAdaPage() {
  return createContentPage({
    contentPath: "content/ada.md",
    accent: 0xff6fae,
  });
}
