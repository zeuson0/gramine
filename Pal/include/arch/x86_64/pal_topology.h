/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2022 Intel Corporation
 *                    Michał Kowalczyk <mkow@invisiblethingslab.com>
 *                    Vijay Dhanraj <vijay.dhanraj@intel.com>
 */

#ifndef PAL_TOPOLOGY_H
#define PAL_TOPOLOGY_H

#include <stdbool.h>

/* Used to represent buffers having numeric values and unit suffixes if present, e.g. "1024576K".
 * NOTE: Used to allocate on stack; increase with caution or use malloc instead. */
#define PAL_SYSFS_BUF_FILESZ (256+2)

/* Max number of caches (such as L1i, L1d, L2, etc.) supported. */
#define MAX_CACHES 4

enum {
    HUGEPAGES_2M = 0,
    HUGEPAGES_1G,
    HUGEPAGES_MAX,
};

static const size_t hugepage_size[] = {
    [HUGEPAGES_2M] = (1 << 21),
    [HUGEPAGES_1G] = (1 << 30),
};

enum cache_type {
    CACHE_TYPE_DATA,
    CACHE_TYPE_INSTRUCTION,
    CACHE_TYPE_UNIFIED,
};

struct pal_cache_info {
    enum cache_type type;
    size_t level;
    size_t size;
    size_t coherency_line_size;
    size_t number_of_sets;
    size_t physical_line_partition;
};

struct pal_cpu_thread_info {
    bool is_online;
    /* Everything below is valid only if the core is online! */

    size_t core_id; // containing core; index into pal_topo_info::cores
    size_t caches_ids[MAX_CACHES]; // indices into pal_topo_info::caches, -1 if not present
};

struct pal_cpu_core_info {
    /* We have our own numbering of physical cores (not taken from the host), so we can just ignore
     * offline cores and thus skip `is_online` here. */

    size_t socket_id;
};

// TODO: move info from struct pal_cpu_info to here
struct pal_socket_info {
    /* We have our own numbering of sockets (not taken from the host), so we can just ignore
     * offline sockets and thus skip `is_online` from here. */

    size_t node_id;
};

struct pal_numa_node_info {
    bool is_online;
    /* Everything below is valid only if the node is online! */

    size_t nr_hugepages[HUGEPAGES_MAX];
};

/* The data in this structure are normalized, to ensure that they can be easily verified for
 * consistency when crossing untrusted -> trusted boundary.
 *
 * It's also kept as flat as possible (no nested pointers), to make mishandling it during
 * sanitization harder.
 *
 * It's supposed to be immutable after initialization, hence no locks are needed (but also no
 * support for hot-plugging CPUs).
 *
 * Note: We can't simplify this to remove sockets (assuming each node has exactly one socket),
 * because qemu likes to put multiple sockets inside a single node.
 */
struct pal_topo_info {
    size_t caches_cnt;
    struct pal_cache_info* caches;

    size_t threads_cnt;
    struct pal_cpu_thread_info* threads;

    size_t cores_cnt;
    struct pal_cpu_core_info* cores;

    size_t sockets_cnt;
    struct pal_socket_info* sockets;

    size_t numa_nodes_cnt;
    struct pal_numa_node_info* numa_nodes;

    /* Has `numa_nodes_cnt` x `numa_nodes_cnt` elements.
     * numa_distance_matrix[i*numa_nodes_cnt + j] is NUMA distance from node i to node j. */
    size_t* numa_distance_matrix;
};

#endif /* PAL_TOPOLOGY_H */
