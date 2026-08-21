import { createContentPage } from "./contentPage.js";

export function createOurTeamPage() {
  return createContentPage({
    contentPath: "content/our-team.md",
    accent: 0xffd166,
  });
}
