/* KIS site — the site graph, transcribed from the concept page (Designer.png).
   One entry per pair (icon + text). `parent` wires the mind-map edges;
   `pos` is the pair's center on the homepage canvas in [x%, y%].

   The four direct children of 'kis' sit at the WarningRing's corners
   instead: `corner` ('tl'/'tr'/'bl'/'br') places them an L-shaped offset
   (out l horizontally, then h vertically — both derived from the ring's
   own measured size plus a fixed gap, mindmap.js) outside the ring's
   corner, so they stay correctly placed if the ring is ever resized —
   no `pos` needed for these four. */

window.KIS = window.KIS || {};

window.KIS.SITE = {
  title: 'KIS — Keep It Simple',
  nodes: [
    { id: 'kis', type: 'logo', pos: [50, 50] },

    { id: 'concepts', parent: 'kis', label: 'Concepts', icon: 'concepts',
      href: 'pages/concepts.html', corner: 'tl',
      summary: 'The ideas the project stands on: what cooperative awareness is, why it matters, and the vocabulary every other page uses.' },
    { id: 'knowledge-base', parent: 'concepts', label: 'Knowledge Base', icon: 'knowledge-base',
      href: 'pages/knowledge-base.html', pos: [30, 16],
      summary: 'Collected background material: platform references, standards notes, and everything worth knowing before reading the plans.' },
    { id: 'requirements', parent: 'concepts', label: 'Requirements', icon: 'requirements',
      href: 'pages/requirements.html', pos: [51, 7],
      summary: 'The enumerated, testable requirements — each with its acceptance check and feasibility verdict.' },
    { id: 'plans', parent: 'concepts', label: 'Plans', icon: 'plans',
      href: 'pages/plans.html', pos: [67, 18],
      summary: 'The phased implementation plans: phases, tasks and subtasks, each traceable back to a requirement.' },

    { id: 'proposal', parent: 'kis', label: 'Proposal', icon: 'proposal',
      href: 'pages/proposal.html', corner: 'tr',
      summary: 'The project proposal: the mission, the scope, and the case made to the stakeholders.' },
    { id: 'proposal-presentation', parent: 'proposal', label: 'Proposal Presentation', icon: 'proposal-presentation',
      href: 'pages/proposal-presentation.html', pos: [90, 12],
      summary: 'The slide deck presenting the proposal — the abridged, human-facing version of the full report.' },

    { id: 'design', parent: 'kis', label: 'Design', icon: 'design',
      href: 'pages/design.html', corner: 'bl',
      summary: 'How the system is designed: the architecture decisions and the documents that record them.' },
    { id: 'system-design', parent: 'design', label: 'System Design', icon: 'system-design',
      href: 'pages/system-design.html', pos: [15, 38],
      summary: 'The system-level view: the nodes, the contracts between them, and the network that joins them.' },
    { id: 'module-design', parent: 'design', label: 'Module Design', icon: 'module-design',
      href: 'pages/module-design.html', pos: [9, 82],
      summary: 'The per-module designs: each component’s responsibility, its seams, and the layer it belongs to.' },
    { id: 'presentation-style', parent: 'module-design', label: 'Presentation Style', icon: 'presentation-style',
      href: 'pages/presentation-style.html', pos: [4, 96],
      summary: 'How module designs are presented as slides: the deck conventions and the templates.' },
    { id: 'article-style', parent: 'module-design', label: 'Article Style', icon: 'article-style',
      href: 'pages/article-style.html', pos: [20, 94],
      summary: 'How module designs are written as documents: the report structure and the writing rules.' },

    { id: 'delivery', parent: 'kis', label: 'Delivery', icon: 'delivery',
      href: 'pages/delivery.html', corner: 'br',
      summary: 'What shipping looks like: how the work is verified, deployed, and proven done.' },
    { id: 'acceptance-evidence', parent: 'delivery', label: 'Acceptance Evidence', icon: 'acceptance-evidence',
      href: 'pages/acceptance-evidence.html', pos: [89, 50],
      summary: 'The evidence that closes each requirement: logs, recordings and CI runs, traced to the acceptance checks.' },
    { id: 'guide-to-get-evidence', parent: 'delivery', label: 'Guide to get Evidence', icon: 'guide',
      href: 'pages/guide-to-get-evidence.html', pos: [84, 72],
      summary: 'The step-by-step walkthroughs a human follows to reproduce the acceptance evidence.' },
  ],
};

window.KIS.SITE.byId = Object.fromEntries(window.KIS.SITE.nodes.map(function (n) { return [n.id, n]; }));
window.KIS.SITE.childrenOf = function (id) {
  return window.KIS.SITE.nodes.filter(function (n) { return n.parent === id; });
};
