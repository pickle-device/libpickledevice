// Copyright (c) 2024 Advanced Micro Devices, Inc.
// Copyright (c) 2025 The Regents of the University of California
// All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause

#include "../../pickle_job.h"

#ifndef WRAPPER_H
#define WRAPPER_H

template <typename Graph, typename TIncomingEdgeSelector, typename TIncomingEdgeConsumer>
PickleJob createGraphJobUsingIncomingEdges(
    const Graph* g,
    TIncomingEdgeSelector incomingEdgeSelector, TIncomingEdgeConsumer incomingEdgeConsumer
)
{
    PickleJob job;

    job.addArrayDescriptor(g->getInIndexArrayDescriptor());
    job.addArrayDescriptor(g->getInNeighborsArrayDescriptor());

    if constexpr (!std::is_same<TIncomingEdgeSelector, std::nullptr_t>::value)
    {
        if (incomingEdgeSelector != nullptr)
        {
            job.addArrayDescriptor(incomingEdgeSelector->getArrayDescriptor());
            g->inIndexIndexedBy(incomingEdgeSelector->getArrayDescriptor());
        }
    }
    if constexpr (!std::is_same<TIncomingEdgeConsumer, std::nullptr_t>::value)
    {
        if (incomingEdgeConsumer != nullptr)
        {
            job.addArrayDescriptor(incomingEdgeConsumer->getArrayDescriptor());
            incomingEdgeConsumer->indexedBy(g->getInNeighborsArrayDescriptor());
        }
    }

    return job;
}

template <typename Graph, typename TOutgoingEdgeSelector, typename TOutgoingEdgeConsumer>
PickleJob createGraphJobUsingOutgoingEdges(
    const Graph* g,
    TOutgoingEdgeSelector outgoingEdgeSelector, TOutgoingEdgeConsumer outgoingEdgeConsumer
)
{
    PickleJob job;

    job.addArrayDescriptor(g->getOutIndexArrayDescriptor());
    job.addArrayDescriptor(g->getOutNeighborsArrayDescriptor());

    if constexpr (!std::is_same<TOutgoingEdgeSelector, std::nullptr_t>::value)
    {
        if (outgoingEdgeSelector != nullptr)
        {
            job.addArrayDescriptor(outgoingEdgeSelector->getArrayDescriptor());
            g->outIndexIndexedBy(outgoingEdgeSelector->getArrayDescriptor());
        }
    }
    if constexpr (!std::is_same<TOutgoingEdgeConsumer, std::nullptr_t>::value)
    {
        if (outgoingEdgeConsumer != nullptr)
        {
            job.addArrayDescriptor(outgoingEdgeConsumer->getArrayDescriptor());
            outgoingEdgeConsumer->indexedBy(g->getOutNeighborsArrayDescriptor());
        }
    }

    return job;
}

#endif // WRAPPER_H
