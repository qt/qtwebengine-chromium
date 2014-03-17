/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/accessibility/AXARIAGridRow.h"

#include "core/accessibility/AXTable.h"

using namespace std;

namespace WebCore {

AXARIAGridRow::AXARIAGridRow(RenderObject* renderer)
    : AXTableRow(renderer)
{
}

AXARIAGridRow::~AXARIAGridRow()
{
}

PassRefPtr<AXARIAGridRow> AXARIAGridRow::create(RenderObject* renderer)
{
    return adoptRef(new AXARIAGridRow(renderer));
}

bool AXARIAGridRow::isARIATreeGridRow() const
{
    AXObject* parent = parentTable();
    if (!parent)
        return false;

    return parent->ariaRoleAttribute() == TreeGridRole;
}

void AXARIAGridRow::disclosedRows(AccessibilityChildrenVector& disclosedRows)
{
    // The contiguous disclosed rows will be the rows in the table that
    // have an aria-level of plus 1 from this row.
    AXObject* parent = parentObjectUnignored();
    if (!parent || !parent->isAXTable())
        return;

    // Search for rows that match the correct level.
    // Only take the subsequent rows from this one that are +1 from this row's level.
    int index = rowIndex();
    if (index < 0)
        return;

    unsigned level = hierarchicalLevel();
    AccessibilityChildrenVector& allRows = toAXTable(parent)->rows();
    int rowCount = allRows.size();
    for (int k = index + 1; k < rowCount; ++k) {
        AXObject* row = allRows[k].get();
        // Stop at the first row that doesn't match the correct level.
        if (row->hierarchicalLevel() != level + 1)
            break;

        disclosedRows.append(row);
    }
}

AXObject* AXARIAGridRow::disclosedByRow() const
{
    // The row that discloses this one is the row in the table
    // that is aria-level subtract 1 from this row.
    AXObject* parent = parentObjectUnignored();
    if (!parent || !parent->isAXTable())
        return 0;

    // If the level is 1 or less, than nothing discloses this row.
    unsigned level = hierarchicalLevel();
    if (level <= 1)
        return 0;

    // Search for the previous row that matches the correct level.
    int index = rowIndex();
    AccessibilityChildrenVector& allRows = toAXTable(parent)->rows();
    int rowCount = allRows.size();
    if (index >= rowCount)
        return 0;

    for (int k = index - 1; k >= 0; --k) {
        AXObject* row = allRows[k].get();
        if (row->hierarchicalLevel() == level - 1)
            return row;
    }

    return 0;
}

AXObject* AXARIAGridRow::headerObject()
{
    AccessibilityChildrenVector rowChildren = children();
    unsigned childrenCount = rowChildren.size();
    for (unsigned i = 0; i < childrenCount; ++i) {
        AXObject* cell = rowChildren[i].get();
        if (cell->ariaRoleAttribute() == RowHeaderRole)
            return cell;
    }

    return 0;
}

} // namespace WebCore
