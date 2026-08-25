import { createContentPage } from "./contentPage.js";

export function createProjectGoalsPage() {
  return createContentPage({
    contentPath: "content/project-goals.md",
    accent: 0xff5c5c,
  });
}
