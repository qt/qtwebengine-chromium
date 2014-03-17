/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/StylePropertySet.h"

#include "RuntimeEnabledFeatures.h"
#include "StylePropertyShorthand.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSValuePool.h"
#include "core/css/CSSVariableValue.h"
#include "core/css/RuntimeCSSEnabled.h"
#include "core/css/StylePropertySerializer.h"
#include "core/css/StyleSheetContents.h"
#include "wtf/text/StringBuilder.h"

#ifndef NDEBUG
#include "wtf/text/CString.h"
#include <stdio.h>
#endif

using namespace std;

namespace WebCore {

static size_t sizeForImmutableStylePropertySetWithPropertyCount(unsigned count)
{
    return sizeof(ImmutableStylePropertySet) - sizeof(void*) + sizeof(CSSValue*) * count + sizeof(StylePropertyMetadata) * count;
}

PassRefPtr<ImmutableStylePropertySet> ImmutableStylePropertySet::create(const CSSProperty* properties, unsigned count, CSSParserMode cssParserMode)
{
    ASSERT(count <= MaxArraySize);
    void* slot = WTF::fastMalloc(sizeForImmutableStylePropertySetWithPropertyCount(count));
    return adoptRef(new (slot) ImmutableStylePropertySet(properties, count, cssParserMode));
}

PassRefPtr<ImmutableStylePropertySet> StylePropertySet::immutableCopyIfNeeded() const
{
    if (!isMutable())
        return toImmutableStylePropertySet(const_cast<StylePropertySet*>(this));
    const MutableStylePropertySet* mutableThis = toMutableStylePropertySet(this);
    return ImmutableStylePropertySet::create(mutableThis->m_propertyVector.data(), mutableThis->m_propertyVector.size(), cssParserMode());
}

MutableStylePropertySet::MutableStylePropertySet(CSSParserMode cssParserMode)
    : StylePropertySet(cssParserMode)
{
}

MutableStylePropertySet::MutableStylePropertySet(const CSSProperty* properties, unsigned length)
    : StylePropertySet(HTMLStandardMode)
{
    m_propertyVector.reserveInitialCapacity(length);
    for (unsigned i = 0; i < length; ++i)
        m_propertyVector.uncheckedAppend(properties[i]);
}

ImmutableStylePropertySet::ImmutableStylePropertySet(const CSSProperty* properties, unsigned length, CSSParserMode cssParserMode)
    : StylePropertySet(cssParserMode, length)
{
    StylePropertyMetadata* metadataArray = const_cast<StylePropertyMetadata*>(this->metadataArray());
    CSSValue** valueArray = const_cast<CSSValue**>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i) {
        metadataArray[i] = properties[i].metadata();
        valueArray[i] = properties[i].value();
        valueArray[i]->ref();
    }
}

ImmutableStylePropertySet::~ImmutableStylePropertySet()
{
    CSSValue** valueArray = const_cast<CSSValue**>(this->valueArray());
    for (unsigned i = 0; i < m_arraySize; ++i)
        valueArray[i]->deref();
}

MutableStylePropertySet::MutableStylePropertySet(const StylePropertySet& other)
    : StylePropertySet(other.cssParserMode())
{
    if (other.isMutable()) {
        m_propertyVector = toMutableStylePropertySet(other).m_propertyVector;
    } else {
        m_propertyVector.reserveInitialCapacity(other.propertyCount());
        for (unsigned i = 0; i < other.propertyCount(); ++i)
            m_propertyVector.uncheckedAppend(other.propertyAt(i).toCSSProperty());
    }
}

String StylePropertySet::getPropertyValue(CSSPropertyID propertyID) const
{
    RefPtr<CSSValue> value = getPropertyCSSValue(propertyID);
    if (value)
        return value->cssText();

    return StylePropertySerializer(*this).getPropertyValue(propertyID);
}

PassRefPtr<CSSValue> StylePropertySet::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return 0;
    return propertyAt(foundPropertyIndex).value();
}

unsigned StylePropertySet::variableCount() const
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    unsigned count = 0;
    for (unsigned i = 0; i < propertyCount(); ++i) {
        if (propertyAt(i).id() == CSSPropertyVariable)
            count++;
    }
    return count;
}

String StylePropertySet::variableValue(const AtomicString& name) const
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    size_t index = findVariableIndex(name);
    if (index == kNotFound)
        return String();
    return toCSSVariableValue(propertyAt(index).value())->value();
}

bool MutableStylePropertySet::removeShorthandProperty(CSSPropertyID propertyID)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    bool ret = removePropertiesInSet(shorthand.properties(), shorthand.length());

    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(propertyID);
    if (prefixingVariant == propertyID)
        return ret;

    StylePropertyShorthand shorthandPrefixingVariant = shorthandForProperty(prefixingVariant);
    return removePropertiesInSet(shorthandPrefixingVariant.properties(), shorthandPrefixingVariant.length());
}

bool MutableStylePropertySet::removeProperty(CSSPropertyID propertyID, String* returnText)
{
    if (removeShorthandProperty(propertyID)) {
        // FIXME: Return an equivalent shorthand when possible.
        if (returnText)
            *returnText = "";
        return true;
    }

    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1) {
        if (returnText)
            *returnText = "";
        return false;
    }

    if (returnText)
        *returnText = propertyAt(foundPropertyIndex).value()->cssText();

    // A more efficient removal strategy would involve marking entries as empty
    // and sweeping them when the vector grows too big.
    m_propertyVector.remove(foundPropertyIndex);

    removePrefixedOrUnprefixedProperty(propertyID);

    return true;
}

void MutableStylePropertySet::removePrefixedOrUnprefixedProperty(CSSPropertyID propertyID)
{
    int foundPropertyIndex = findPropertyIndex(prefixingVariantForPropertyId(propertyID));
    if (foundPropertyIndex == -1)
        return;
    m_propertyVector.remove(foundPropertyIndex);
}

bool StylePropertySet::propertyIsImportant(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();

    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length())
        return false;

    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (!propertyIsImportant(shorthand.properties()[i]))
            return false;
    }
    return true;
}

CSSPropertyID StylePropertySet::getPropertyShorthand(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return CSSPropertyInvalid;
    return propertyAt(foundPropertyIndex).shorthandID();
}

bool StylePropertySet::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

bool MutableStylePropertySet::setProperty(CSSPropertyID propertyID, const String& value, bool important, StyleSheetContents* contextStyleSheet)
{
    // Setting the value to an empty string just removes the property in both IE and Gecko.
    // Setting it to null seems to produce less consistent results, but we treat it just the same.
    if (value.isEmpty())
        return removeProperty(propertyID);

    // When replacing an existing property value, this moves the property to the end of the list.
    // Firefox preserves the position, and MSIE moves the property to the beginning.
    return CSSParser::parseValue(this, propertyID, value, important, cssParserMode(), contextStyleSheet);
}

void MutableStylePropertySet::setProperty(CSSPropertyID propertyID, PassRefPtr<CSSValue> prpValue, bool important)
{
    StylePropertyShorthand shorthand = shorthandForProperty(propertyID);
    if (!shorthand.length()) {
        setProperty(CSSProperty(propertyID, prpValue, important));
        return;
    }

    removePropertiesInSet(shorthand.properties(), shorthand.length());

    RefPtr<CSSValue> value = prpValue;
    for (unsigned i = 0; i < shorthand.length(); ++i)
        m_propertyVector.append(CSSProperty(shorthand.properties()[i], value, important));
}

void MutableStylePropertySet::setProperty(const CSSProperty& property, CSSProperty* slot)
{
    if (!removeShorthandProperty(property.id())) {
        CSSProperty* toReplace = slot ? slot : findCSSPropertyWithID(property.id());
        if (toReplace) {
            *toReplace = property;
            setPrefixingVariantProperty(property);
            return;
        }
    }
    appendPrefixingVariantProperty(property);
}

unsigned getIndexInShorthandVectorForPrefixingVariant(const CSSProperty& property, CSSPropertyID prefixingVariant)
{
    if (!property.isSetFromShorthand())
        return 0;

    CSSPropertyID prefixedShorthand = prefixingVariantForPropertyId(property.shorthandID());
    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(prefixingVariant, &shorthands);
    return indexOfShorthandForLonghand(prefixedShorthand, shorthands);
}

bool MutableStylePropertySet::setVariableValue(const AtomicString& name, const String& value, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    if (value.isEmpty())
        return removeVariable(name);

    size_t index = findVariableIndex(name);
    if (index != kNotFound) {
        const CSSValue* cssValue = m_propertyVector.at(index).value();
        if (toCSSVariableValue(cssValue)->value() == value)
            return false;
    }

    CSSProperty property(CSSPropertyVariable, CSSVariableValue::create(name, value), important);
    if (index == kNotFound) {
        m_propertyVector.append(property);
        return true;
    }
    m_propertyVector.at(index) = property;
    return false;
}

void MutableStylePropertySet::appendPrefixingVariantProperty(const CSSProperty& property)
{
    m_propertyVector.append(property);
    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(property.id());
    if (prefixingVariant == property.id())
        return;

    m_propertyVector.append(CSSProperty(prefixingVariant, property.value(), property.isImportant(), property.isSetFromShorthand(), getIndexInShorthandVectorForPrefixingVariant(property, prefixingVariant), property.metadata().m_implicit));
}

void MutableStylePropertySet::setPrefixingVariantProperty(const CSSProperty& property)
{
    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(property.id());
    CSSProperty* toReplace = findCSSPropertyWithID(prefixingVariant);
    if (toReplace && prefixingVariant != property.id())
        *toReplace = CSSProperty(prefixingVariant, property.value(), property.isImportant(), property.isSetFromShorthand(), getIndexInShorthandVectorForPrefixingVariant(property, prefixingVariant), property.metadata().m_implicit);
}

bool MutableStylePropertySet::setProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    setProperty(CSSProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important));
    return true;
}

bool MutableStylePropertySet::setProperty(CSSPropertyID propertyID, CSSPropertyID identifier, bool important)
{
    setProperty(CSSProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important));
    return true;
}

void MutableStylePropertySet::parseDeclaration(const String& styleDeclaration, StyleSheetContents* contextStyleSheet)
{
    m_propertyVector.clear();

    CSSParserContext context(cssParserMode());
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.setMode(cssParserMode());
    }

    CSSParser parser(context, UseCounter::getFrom(contextStyleSheet));
    parser.parseDeclaration(this, styleDeclaration, 0, contextStyleSheet);
}

void MutableStylePropertySet::addParsedProperties(const Vector<CSSProperty, 256>& properties)
{
    m_propertyVector.reserveCapacity(m_propertyVector.size() + properties.size());
    for (unsigned i = 0; i < properties.size(); ++i)
        addParsedProperty(properties[i]);
}

void MutableStylePropertySet::addParsedProperty(const CSSProperty& property)
{
    // Only add properties that have no !important counterpart present
    if (!propertyIsImportant(property.id()) || property.isImportant())
        setProperty(property);
}

String StylePropertySet::asText() const
{
    return StylePropertySerializer(*this).asText();
}

bool StylePropertySet::hasCSSOMWrapper() const
{
    return m_isMutable && toMutableStylePropertySet(this)->m_cssomWrapper;
}

void MutableStylePropertySet::mergeAndOverrideOnConflict(const StylePropertySet* other)
{
    unsigned size = other->propertyCount();
    for (unsigned n = 0; n < size; ++n) {
        PropertyReference toMerge = other->propertyAt(n);
        CSSProperty* old = findCSSPropertyWithID(toMerge.id());
        if (old)
            setProperty(toMerge.toCSSProperty(), old);
        else
            appendPrefixingVariantProperty(toMerge.toCSSProperty());
    }
}

void StylePropertySet::addSubresourceStyleURLs(ListHashSet<KURL>& urls, StyleSheetContents* contextStyleSheet) const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i)
        propertyAt(i).value()->addSubresourceStyleURLs(urls, contextStyleSheet);
}

bool StylePropertySet::hasFailedOrCanceledSubresources() const
{
    unsigned size = propertyCount();
    for (unsigned i = 0; i < size; ++i) {
        if (propertyAt(i).value()->hasFailedOrCanceledSubresources())
            return true;
    }
    return false;
}

// This is the list of properties we want to copy in the copyBlockProperties() function.
// It is the list of CSS properties that apply specially to block-level elements.
static const CSSPropertyID staticBlockProperties[] = {
    CSSPropertyOrphans,
    CSSPropertyOverflow, // This can be also be applied to replaced elements
    CSSPropertyWebkitAspectRatio,
    CSSPropertyWebkitColumnCount,
    CSSPropertyWebkitColumnGap,
    CSSPropertyWebkitColumnRuleColor,
    CSSPropertyWebkitColumnRuleStyle,
    CSSPropertyWebkitColumnRuleWidth,
    CSSPropertyWebkitColumnBreakBefore,
    CSSPropertyWebkitColumnBreakAfter,
    CSSPropertyWebkitColumnBreakInside,
    CSSPropertyWebkitColumnWidth,
    CSSPropertyPageBreakAfter,
    CSSPropertyPageBreakBefore,
    CSSPropertyPageBreakInside,
    CSSPropertyWebkitRegionBreakAfter,
    CSSPropertyWebkitRegionBreakBefore,
    CSSPropertyWebkitRegionBreakInside,
    CSSPropertyTextAlign,
    CSSPropertyTextAlignLast,
    CSSPropertyTextIndent,
    CSSPropertyTextJustify,
    CSSPropertyWidows
};

static const Vector<CSSPropertyID>& blockProperties()
{
    DEFINE_STATIC_LOCAL(Vector<CSSPropertyID>, properties, ());
    if (properties.isEmpty())
        RuntimeCSSEnabled::filterEnabledCSSPropertiesIntoVector(staticBlockProperties, WTF_ARRAY_LENGTH(staticBlockProperties), properties);
    return properties;
}

void MutableStylePropertySet::clear()
{
    m_propertyVector.clear();
}

PassRefPtr<MutableStylePropertySet> StylePropertySet::copyBlockProperties() const
{
    return copyPropertiesInSet(blockProperties());
}

void MutableStylePropertySet::removeBlockProperties()
{
    removePropertiesInSet(blockProperties().data(), blockProperties().size());
}

bool MutableStylePropertySet::removePropertiesInSet(const CSSPropertyID* set, unsigned length)
{
    if (m_propertyVector.isEmpty())
        return false;

    // FIXME: This is always used with static sets and in that case constructing the hash repeatedly is pretty pointless.
    HashSet<CSSPropertyID> toRemove;
    for (unsigned i = 0; i < length; ++i)
        toRemove.add(set[i]);

    Vector<CSSProperty> newProperties;
    newProperties.reserveInitialCapacity(m_propertyVector.size());

    unsigned size = m_propertyVector.size();
    for (unsigned n = 0; n < size; ++n) {
        const CSSProperty& property = m_propertyVector.at(n);
        // Not quite sure if the isImportant test is needed but it matches the existing behavior.
        if (!property.isImportant()) {
            if (toRemove.contains(property.id()))
                continue;
        }
        newProperties.append(property);
    }

    bool changed = newProperties.size() != m_propertyVector.size();
    m_propertyVector = newProperties;
    return changed;
}

int StylePropertySet::findPropertyIndex(CSSPropertyID propertyID) const
{
    // Convert here propertyID into an uint16_t to compare it with the metadata's m_propertyID to avoid
    // the compiler converting it to an int multiple times in the loop.
    uint16_t id = static_cast<uint16_t>(propertyID);
    for (int n = propertyCount() - 1 ; n >= 0; --n) {
        if (id == propertyAt(n).propertyMetadata().m_propertyID) {
            // Only enabled or internal properties should be part of the style.
            ASSERT(RuntimeCSSEnabled::isCSSPropertyEnabled(propertyID) || isInternalProperty(propertyID));
            return n;
        }
    }
    return -1;
}

size_t StylePropertySet::findVariableIndex(const AtomicString& name) const
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    for (int i = propertyCount() - 1; i >= 0; --i) {
        const PropertyReference& property = propertyAt(i);
        if (property.id() == CSSPropertyVariable && toCSSVariableValue(property.value())->name() == name)
            return i;
    }
    return kNotFound;
}

CSSProperty* MutableStylePropertySet::findCSSPropertyWithID(CSSPropertyID propertyID)
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return 0;
    return &m_propertyVector.at(foundPropertyIndex);
}

bool StylePropertySet::propertyMatches(CSSPropertyID propertyID, const CSSValue* propertyValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).value()->equals(*propertyValue);
}

void MutableStylePropertySet::removeEquivalentProperties(const StylePropertySet* style)
{
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = m_propertyVector.size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->propertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

void MutableStylePropertySet::removeEquivalentProperties(const CSSStyleDeclaration* style)
{
    Vector<CSSPropertyID> propertiesToRemove;
    unsigned size = m_propertyVector.size();
    for (unsigned i = 0; i < size; ++i) {
        PropertyReference property = propertyAt(i);
        if (style->cssPropertyMatches(property.id(), property.value()))
            propertiesToRemove.append(property.id());
    }
    // FIXME: This should use mass removal.
    for (unsigned i = 0; i < propertiesToRemove.size(); ++i)
        removeProperty(propertiesToRemove[i]);
}

bool MutableStylePropertySet::removeVariable(const AtomicString& name)
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    size_t index = findVariableIndex(name);
    if (index == kNotFound)
        return false;
    m_propertyVector.remove(index);
    return true;
}

bool MutableStylePropertySet::clearVariables()
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    CSSPropertyID variablesId = CSSPropertyVariable;
    return removePropertiesInSet(&variablesId, 1);
}

PassRefPtr<MutableStylePropertySet::VariablesIterator> MutableStylePropertySet::VariablesIterator::create(MutableStylePropertySet* propertySet)
{
    ASSERT(RuntimeEnabledFeatures::cssVariablesEnabled());
    const size_t propertyCount = propertySet->propertyCount();
    size_t variableCount = 0;
    Vector<AtomicString> remainingNames(propertyCount);
    for (int i = propertyCount; i--; ) {
        const PropertyReference& property = propertySet->propertyAt(i);
        if (property.id() == CSSPropertyVariable)
            remainingNames[variableCount++] = toCSSVariableValue(property.value())->name();
    }
    remainingNames.shrink(variableCount);

    RefPtr<VariablesIterator> iterator = adoptRef(new VariablesIterator(propertySet));
    // FIXME: Make use of the Vector move constructor when rvalues are supported on all platforms.
    iterator->takeRemainingNames(remainingNames);
    return iterator.release();
}

void MutableStylePropertySet::VariablesIterator::addedVariable(const AtomicString& name)
{
    ASSERT(!m_remainingNames.contains(name));
    ASSERT(!m_newNames.contains(name));
    m_newNames.append(name);
}

void MutableStylePropertySet::VariablesIterator::removedVariable(const AtomicString& name)
{
    size_t index = m_remainingNames.find(name);
    if (index != kNotFound)
        m_remainingNames.remove(index);
    index = m_newNames.find(name);
    if (index != kNotFound)
        m_newNames.remove(index);
}

void MutableStylePropertySet::VariablesIterator::clearedVariables()
{
    m_remainingNames.clear();
    m_newNames.clear();
}

void MutableStylePropertySet::VariablesIterator::advance()
{
    if (!atEnd())
        m_remainingNames.removeLast();
    if (!m_newNames.isEmpty()) {
        m_remainingNames.appendVector(m_newNames);
        m_newNames.clear();
    }
}

PassRefPtr<MutableStylePropertySet> StylePropertySet::mutableCopy() const
{
    return adoptRef(new MutableStylePropertySet(*this));
}

PassRefPtr<MutableStylePropertySet> StylePropertySet::copyPropertiesInSet(const Vector<CSSPropertyID>& properties) const
{
    Vector<CSSProperty, 256> list;
    list.reserveInitialCapacity(properties.size());
    for (unsigned i = 0; i < properties.size(); ++i) {
        RefPtr<CSSValue> value = getPropertyCSSValue(properties[i]);
        if (value)
            list.append(CSSProperty(properties[i], value.release(), false));
    }
    return MutableStylePropertySet::create(list.data(), list.size());
}

PropertySetCSSStyleDeclaration* MutableStylePropertySet::cssStyleDeclaration()
{
    return m_cssomWrapper.get();
}

CSSStyleDeclaration* MutableStylePropertySet::ensureCSSStyleDeclaration()
{
    if (m_cssomWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(m_cssomWrapper.get())->parentRule());
        ASSERT(!m_cssomWrapper->parentElement());
        return m_cssomWrapper.get();
    }
    m_cssomWrapper = adoptPtr(new PropertySetCSSStyleDeclaration(this));
    return m_cssomWrapper.get();
}

CSSStyleDeclaration* MutableStylePropertySet::ensureInlineCSSStyleDeclaration(Element* parentElement)
{
    if (m_cssomWrapper) {
        ASSERT(m_cssomWrapper->parentElement() == parentElement);
        return m_cssomWrapper.get();
    }
    m_cssomWrapper = adoptPtr(new InlineCSSStyleDeclaration(this, parentElement));
    return m_cssomWrapper.get();
}

unsigned StylePropertySet::averageSizeInBytes()
{
    // Please update this if the storage scheme changes so that this longer reflects the actual size.
    return sizeForImmutableStylePropertySetWithPropertyCount(4);
}

// See the function above if you need to update this.
struct SameSizeAsStylePropertySet : public RefCounted<SameSizeAsStylePropertySet> {
    unsigned bitfield;
};
COMPILE_ASSERT(sizeof(StylePropertySet) == sizeof(SameSizeAsStylePropertySet), style_property_set_should_stay_small);

#ifndef NDEBUG
void StylePropertySet::showStyle()
{
    fprintf(stderr, "%s\n", asText().ascii().data());
}
#endif

PassRefPtr<MutableStylePropertySet> MutableStylePropertySet::create(CSSParserMode cssParserMode)
{
    return adoptRef(new MutableStylePropertySet(cssParserMode));
}

PassRefPtr<MutableStylePropertySet> MutableStylePropertySet::create(const CSSProperty* properties, unsigned count)
{
    return adoptRef(new MutableStylePropertySet(properties, count));
}

String StylePropertySet::PropertyReference::cssName() const
{
    if (id() == CSSPropertyVariable) {
        if (!propertyValue()->isVariableValue())
            return emptyString(); // Should not happen, but if it does, avoid a bad cast.
        return "var-" + toCSSVariableValue(propertyValue())->name();
    }
    return getPropertyNameString(id());
}

String StylePropertySet::PropertyReference::cssText() const
{
    StringBuilder result;
    result.append(cssName());
    result.appendLiteral(": ");
    result.append(propertyValue()->cssText());
    if (isImportant())
        result.appendLiteral(" !important");
    result.append(';');
    return result.toString();
}


} // namespace WebCore
