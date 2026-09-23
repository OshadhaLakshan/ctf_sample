import type { Example, Spec } from './types';

/** Builds a complete versioned spec for the built-in editable challenge library. */
function spec(
  category: string,
  type: string,
  input: Spec['input'],
  parameters: Spec['parameters'] = {},
): Spec {
  return {
    version: '1.0',
    category,
    problem: { type },
    input,
    parameters,
    output: { type: 'result' },
  };
}

/** Small reproducible challenges exercise the actual backend without model downloads. */
export const examples: Example[] = [
  {
    id: 'graph',
    title: 'Ghost route',
    category: 'Graph theory',
    tag: 'DIJKSTRA',
    description:
      'Find the lowest-cost route from node A to node F through a compromised relay network. Verify the path and its total cost independently.',
    spec: spec(
      'graph',
      'shortest_path',
      {
        nodes: 6,
        directed: false,
        edges: [
          [0, 1, 4],
          [0, 2, 2],
          [1, 2, 1],
          [1, 3, 5],
          [2, 3, 8],
          [2, 4, 10],
          [3, 4, 2],
          [3, 5, 6],
          [4, 5, 3],
        ],
      },
      { source: 0, target: 5 },
    ),
  },
  {
    id: 'encoding',
    title: 'Signal in the noise',
    category: 'Cryptography',
    tag: 'BASE64',
    description:
      'Decode the intercepted Base64 signal. Confirm the recovered bytes by encoding them back to the original input.',
    spec: spec('crypto', 'base64_decode', { text: 'Q1RGe3RydXN0X2J1dF92ZXJpZnl9' }),
  },
  {
    id: 'network',
    title: 'Subnet zero',
    category: 'Networking',
    tag: 'CIDR',
    description:
      'Calculate the network, broadcast address, usable host range, and subnet mask for 10.42.13.37/20.',
    spec: spec('network', 'cidr_subnet', { cidr: '10.42.13.37/20' }),
  },
  {
    id: 'linux',
    title: 'Permission denied',
    category: 'Linux systems',
    tag: 'POSIX',
    description:
      'Inspect Unix mode 4755. Identify the symbolic permissions and special privilege bits.',
    spec: spec('linux', 'permission_analyze', { mode: '4755' }),
  },
  {
    id: 'mst',
    title: 'Minimum exposure',
    category: 'Graph theory',
    tag: 'KRUSKAL',
    description:
      'Connect every relay with the minimum total link cost. Cross-check the spanning tree with an independent Prim implementation.',
    spec: spec('graph', 'mst', {
      nodes: 5,
      edges: [
        [0, 1, 4],
        [0, 2, 2],
        [1, 2, 1],
        [1, 3, 5],
        [2, 3, 8],
        [3, 4, 2],
        [2, 4, 10],
      ],
    }),
  },
  {
    id: 'lab',
    title: 'The hidden transmission',
    category: 'Multi-stage lab',
    tag: 'AGENT',
    description:
      'Retrieve an encoded signal from the local fixture, decode it, and verify the flag against the recorded evidence. Requires the bundled lab on port 8090.',
    actions: [
      {
        action: 'HTTP_GET',
        arguments: { path: '/challenge' },
        reason: 'Retrieve the signal from the authorized local fixture.',
      },
      {
        action: 'BASE64_DECODE',
        arguments: { from_previous: true },
        reason: 'Decode the actual response from the preceding tool.',
      },
      {
        action: 'VERIFY_FLAG',
        arguments: { from_previous: true },
        reason: 'Check the flag format and supporting tool evidence.',
      },
    ],
  },
];
