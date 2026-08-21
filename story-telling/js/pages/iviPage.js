import { createContentPage } from "./contentPage.js";

export function createIviPage() {
  return createContentPage({
    contentPath: "content/ivi.md",
    accent: 0xff8a5b,
  });
}
