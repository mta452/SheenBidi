/*
 * Copyright (C) 2016 Muhammad Tayyab Akram
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <SBConfig.h>
#include <SBTypes.h>

#include <stddef.h>
#include <stdlib.h>

#include "SBAssert.h"
#include "SBBidiLink.h"
#include "SBCharType.h"
#include "SBBracketQueue.h"

static void _SBBracketQueueFinalizePairs(SBBracketQueueRef queue, _SBBracketQueueListRef list, SBUInteger top)
{
    SBUInteger limit = 0;

    do {
        limit = (list == queue->_rearList ? queue->_rearTop : _SB_BRACKET_QUEUE_LIST__MAX_INDEX);

        while (++top <= limit) {
            if (list->openingLink[top] && !list->closingLink[top]) {
                list->openingLink[top] = NULL;
            }
        };

        list = list->next;
        top = 0;
    } while (list);
}

SB_INTERNAL void SBBracketQueueInitialize(SBBracketQueueRef queue)
{
    queue->_firstList.previous = NULL;
    queue->_firstList.next = NULL;
    queue->_frontList = NULL;
    queue->_rearList = NULL;
    queue->count = 0;
    queue->shouldDequeue = SBFalse;
}

SB_INTERNAL void SBBracketQueueReset(SBBracketQueueRef queue, SBCharType direction)
{
    queue->_frontList = &queue->_firstList;
    queue->_rearList = &queue->_firstList;
    queue->_frontTop = 0;
    queue->_rearTop = SBInvalidIndex;
    queue->count = 0;
    queue->shouldDequeue = SBFalse;
    queue->_direction = direction;
}

SB_INTERNAL void SBBracketQueueEnqueue(SBBracketQueueRef queue, SBBidiLinkRef priorStrongLink, SBBidiLinkRef openingLink, SBCodepoint bracket)
{
    _SBBracketQueueListRef list;
    SBUInteger top;

    if (queue->_rearTop != _SB_BRACKET_QUEUE_LIST__MAX_INDEX) {
        list = queue->_rearList;
        top = ++queue->_rearTop;
    } else {
        _SBBracketQueueListRef rearList;

        rearList = queue->_rearList;
        list = rearList->next;

        if (!list) {
            list = malloc(sizeof(_SBBracketQueueList));
            list->previous = rearList;
            list->next = NULL;

            rearList->next = list;
        }

        queue->_rearList = list;
        queue->_rearTop = top = 0;
    }
    ++queue->count;

    list->priorStrongLink[top] = priorStrongLink;
    list->openingLink[top] = openingLink;
    list->closingLink[top] = NULL;
    list->bracket[top] = bracket;
    list->strongType[top] = SBCharTypeNil;
}

SB_INTERNAL void SBBracketQueueDequeue(SBBracketQueueRef queue)
{
    /* The queue must NOT be empty. */
    SBAssert(queue->count != 0);

    if (queue->_frontTop != _SB_BRACKET_QUEUE_LIST__MAX_INDEX) {
        ++queue->_frontTop;
    } else {
        _SBBracketQueueListRef frontList = queue->_frontList;
        if (frontList == queue->_rearList) {
            queue->_rearTop = SBInvalidIndex;
        } else {
            queue->_frontList = frontList->next;
        }

        queue->_frontTop = 0;
    }
    --queue->count;
}

SB_INTERNAL void SBBracketQueueSetStrongType(SBBracketQueueRef queue, SBCharType strongType)
{
    _SBBracketQueueListRef list = queue->_rearList;
    SBUInteger top = queue->_rearTop;
    SBUInteger limit = 0;

    for (; ;) {
        limit = (list == queue->_frontList ? queue->_frontTop : 0);

        do {
            if (list->closingLink[top] == NULL && list->strongType[top] != queue->_direction) {
                list->strongType[top] = strongType;
            }
        } while (top-- > limit);

        if (list == queue->_frontList) {
            break;
        }

        list = list->previous;
        top = _SB_BRACKET_QUEUE_LIST__MAX_INDEX;
    };
}

SB_INTERNAL void SBBracketQueueClosePair(SBBracketQueueRef queue, SBBidiLinkRef closingLink, SBCodepoint bracket)
{
    _SBBracketQueueListRef list = queue->_rearList;
    SBUInteger top = queue->_rearTop;
    SBUInteger limit = 0;

    for (; ;) {
        SBBoolean is_frontList = (list == queue->_frontList);
        limit = (is_frontList ? queue->_frontTop : 0);

        do {
            if (list->openingLink[top] && !list->closingLink[top]
                && list->bracket[top] == bracket) {
                list->closingLink[top] = closingLink;
                _SBBracketQueueFinalizePairs(queue, list, top);

                if (is_frontList && top == queue->_frontTop) {
                    queue->shouldDequeue = SBTrue;
                }

                return;
            }
        } while (top-- > limit);

        if (is_frontList) {
            break;
        }

        list = list->previous;
        top = _SB_BRACKET_QUEUE_LIST__MAX_INDEX;
    };
}

SB_INTERNAL SBBoolean SBBracketQueueShouldDequeue(SBBracketQueueRef queue)
{
    return queue->shouldDequeue;
}

SB_INTERNAL SBBidiLinkRef SBBracketQueueGetPriorStrongLink(SBBracketQueueRef queue)
{
    return queue->_frontList->priorStrongLink[queue->_frontTop];
}

SB_INTERNAL SBBidiLinkRef SBBracketQueueGetOpeningLink(SBBracketQueueRef queue)
{
    return queue->_frontList->openingLink[queue->_frontTop];
}

SB_INTERNAL SBBidiLinkRef SBBracketQueueGetClosingLink(SBBracketQueueRef queue)
{
    return queue->_frontList->closingLink[queue->_frontTop];
}

SB_INTERNAL SBCharType SBBracketQueueGetStrongType(SBBracketQueueRef queue)
{
    return queue->_frontList->strongType[queue->_frontTop];
}

SB_INTERNAL void SBBracketQueueFinalize(SBBracketQueueRef queue)
{
    _SBBracketQueueListRef list = queue->_firstList.next;
    
    while (list) {
        _SBBracketQueueListRef next = list->next;
        free(list);
        
        list = next;
    };
}
