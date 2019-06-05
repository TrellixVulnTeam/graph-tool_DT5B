// graph-tool -- a general graph modification and manipulation thingy
//
// Copyright (C) 2006-2019 Tiago de Paula Peixoto <tiago@skewed.de>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 3
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#ifndef GRAPH_PARALLEL_HH
#define GRAPH_PARALLEL_HH

#include "hash_map_wrap.hh"
#include "graph_util.hh"
#include "idx_map.hh"

#include <boost/range/adaptor/reversed.hpp>

namespace graph_tool
{
using namespace std;
using namespace boost;

// label parallel edges in the order they are found, starting from 1
struct label_parallel_edges
{
    template <class Graph, class ParallelMap>
    void operator()(const Graph& g, ParallelMap parallel, bool mark_only) const
    {
        typedef typename graph_traits<Graph>::vertex_descriptor vertex_t;
        typedef typename graph_traits<Graph>::edge_descriptor edge_t;
        typename property_map<Graph, edge_index_t>::type eidx = get(edge_index, g);

        idx_map<vertex_t, edge_t> vset;
        idx_map<size_t, bool> self_loops;

        #pragma omp parallel if (num_vertices(g) > OPENMP_MIN_THRESH) \
            firstprivate(vset) firstprivate(self_loops)
        parallel_vertex_loop_no_spawn
            (g,
             [&](auto v)
             {
                 for (auto e : out_edges_range(v, g))
                 {
                     vertex_t u = target(e, g);

                     // do not visit edges twice in undirected graphs
                     if (!graph_tool::is_directed(g) && u < v)
                         continue;

                     if (u == v)
                     {
                         if (self_loops[eidx[e]])
                             continue;
                         self_loops[eidx[e]] = true;
                     }

                     auto iter = vset.find(u);
                     if (iter == vset.end())
                     {
                         vset[u] = e;
                     }
                     else
                     {
                         if (mark_only)
                         {
                             parallel[e] = true;
                         }
                         else
                         {
                             parallel[e] = parallel[iter->second] + 1;
                             vset[u] = e;
                         }
                     }
                 }
                 vset.clear();
                 self_loops.clear();
             });
    }
};

// label self loops edges in the order they are found, starting from 1
struct label_self_loops
{
    template <class Graph, class SelfMap>
    void operator()(const Graph& g, SelfMap self, bool mark_only) const
    {
        parallel_vertex_loop
            (g,
             [&](auto v)
             {
                 size_t n = 1;
                 for (auto e : out_edges_range(v, g))
                 {
                     if (target(e, g) == v)
                         put(self, e, mark_only ? 1 : n++);
                     else
                    put(self, e, 0);
                 }
             });
    }
};

// remove edges with label larger than 0
struct remove_labeled_edges
{
    template <class Graph, class LabelMap>
    void operator()(Graph& g, LabelMap label) const
    {
        typedef typename graph_traits<Graph>::edge_descriptor edge_t;
        vector<edge_t> r_edges;
        for (auto v : vertices_range(g))
        {
            for (auto e : out_edges_range(v, g))
            {
                if (label[e] > 0)
                    r_edges.push_back(e);
            }

            while (!r_edges.empty())
            {
                remove_edge(r_edges.back(), g);
                r_edges.pop_back();
            }
        }
    }
};

} // graph_tool namespace

#endif //GRAPH_PARALLEL_HH
