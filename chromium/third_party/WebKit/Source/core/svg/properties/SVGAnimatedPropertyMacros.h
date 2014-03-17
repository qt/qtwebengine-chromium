/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

#ifndef SVGAnimatedPropertyMacros_h
#define SVGAnimatedPropertyMacros_h

#include "core/dom/Element.h"
#include "core/svg/properties/SVGAnimatedProperty.h"
#include "core/svg/properties/SVGAttributeToPropertyMap.h"
#include "core/svg/properties/SVGPropertyTraits.h"
#include "wtf/StdLibExtras.h"

namespace WebCore {

// SVGSynchronizableAnimatedProperty implementation
template<typename PropertyType>
struct SVGSynchronizableAnimatedProperty {
    SVGSynchronizableAnimatedProperty()
        : value(SVGPropertyTraits<PropertyType>::initialValue())
        , shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameter1>
    SVGSynchronizableAnimatedProperty(const ConstructorParameter1& value1)
        : value(value1)
        , shouldSynchronize(false)
    {
    }

    template<typename ConstructorParameter1, typename ConstructorParameter2>
    SVGSynchronizableAnimatedProperty(const ConstructorParameter1& value1, const ConstructorParameter2& value2)
        : value(value1, value2)
        , shouldSynchronize(false)
    {
    }

    void synchronize(Element* ownerElement, const QualifiedName& attrName, const AtomicString& value)
    {
        ownerElement->setSynchronizedLazyAttribute(attrName, value);
    }

    PropertyType value;
    bool shouldSynchronize : 1;
};

// Property registration helpers
#define BEGIN_REGISTER_ANIMATED_PROPERTIES(OwnerType) \
SVGAttributeToPropertyMap& OwnerType::attributeToPropertyMap() \
{ \
    DEFINE_STATIC_LOCAL(SVGAttributeToPropertyMap, s_attributeToPropertyMap, ()); \
    return s_attributeToPropertyMap; \
} \
\
SVGAttributeToPropertyMap& OwnerType::localAttributeToPropertyMap() const \
{ \
    return attributeToPropertyMap(); \
} \
\
void OwnerType::registerAnimatedPropertiesFor##OwnerType() \
{ \
    OwnerType::m_cleanupAnimatedPropertiesCaller.setOwner(this); \
    SVGAttributeToPropertyMap& map = OwnerType::attributeToPropertyMap(); \
    if (!map.isEmpty()) \
        return; \
    typedef OwnerType UseOwnerType;

#define REGISTER_LOCAL_ANIMATED_PROPERTY(LowerProperty) \
     map.addProperty(UseOwnerType::LowerProperty##PropertyInfo());

#define REGISTER_PARENT_ANIMATED_PROPERTIES(ClassName) \
     map.addProperties(ClassName::attributeToPropertyMap()); \

#define END_REGISTER_ANIMATED_PROPERTIES }

// Property definition helpers (used in SVG*.cpp files)
#define DEFINE_ANIMATED_PROPERTY(AnimatedPropertyTypeEnum, OwnerType, DOMAttribute, SVGDOMAttributeIdentifier, UpperProperty, LowerProperty, TearOffType, PropertyType) \
const SVGPropertyInfo* OwnerType::LowerProperty##PropertyInfo() { \
    DEFINE_STATIC_LOCAL(const SVGPropertyInfo, s_propertyInfo, \
                        (AnimatedPropertyTypeEnum, \
                         PropertyIsReadWrite, \
                         DOMAttribute, \
                         SVGDOMAttributeIdentifier, \
                         &OwnerType::synchronize##UpperProperty, \
                         &OwnerType::lookupOrCreate##UpperProperty##Wrapper)); \
    return &s_propertyInfo; \
} \
PropertyType& OwnerType::LowerProperty##CurrentValue() const \
{ \
    if (TearOffType* wrapper = SVGAnimatedProperty::lookupWrapper<UseOwnerType, TearOffType>(this, LowerProperty##PropertyInfo())) { \
        if (wrapper->isAnimating()) \
            return wrapper->currentAnimatedValue(); \
    } \
    return m_##LowerProperty.value; \
} \
\
PropertyType& OwnerType::LowerProperty##BaseValue() const \
{ \
    return m_##LowerProperty.value; \
} \
\
void OwnerType::set##UpperProperty##BaseValue(const PropertyType& type) \
{ \
    m_##LowerProperty.value = type; \
} \
\
PassRefPtr<TearOffType> OwnerType::LowerProperty() \
{ \
    m_##LowerProperty.shouldSynchronize = true; \
    return static_pointer_cast<TearOffType>(lookupOrCreate##UpperProperty##Wrapper(this)); \
} \
\
void OwnerType::synchronize##UpperProperty() \
{ \
    if (!m_##LowerProperty.shouldSynchronize) \
        return; \
    AtomicString value(SVGPropertyTraits<PropertyType>::toString(m_##LowerProperty.value)); \
    m_##LowerProperty.synchronize(this, LowerProperty##PropertyInfo()->attributeName, value); \
} \
\
PassRefPtr<SVGAnimatedProperty> OwnerType::lookupOrCreate##UpperProperty##Wrapper(SVGElement* maskedOwnerType) \
{ \
    ASSERT(maskedOwnerType); \
    UseOwnerType* ownerType = static_cast<UseOwnerType*>(maskedOwnerType); \
    return SVGAnimatedProperty::lookupOrCreateWrapper<UseOwnerType, TearOffType, PropertyType>(ownerType, LowerProperty##PropertyInfo(), ownerType->m_##LowerProperty.value); \
} \
\
void OwnerType::synchronize##UpperProperty(SVGElement* maskedOwnerType) \
{ \
    ASSERT(maskedOwnerType); \
    UseOwnerType* ownerType = static_cast<UseOwnerType*>(maskedOwnerType); \
    ownerType->synchronize##UpperProperty(); \
}

// Property declaration helpers (used in SVG*.h files)
#define BEGIN_DECLARE_ANIMATED_PROPERTIES(OwnerType) \
public: \
    static SVGAttributeToPropertyMap& attributeToPropertyMap(); \
    virtual SVGAttributeToPropertyMap& localAttributeToPropertyMap() const; \
    void registerAnimatedPropertiesFor##OwnerType(); \
    typedef OwnerType UseOwnerType;

#define DECLARE_ANIMATED_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty) \
public: \
    static const SVGPropertyInfo* LowerProperty##PropertyInfo(); \
    PropertyType& LowerProperty##CurrentValue() const; \
    PropertyType& LowerProperty##BaseValue() const; \
    void set##UpperProperty##BaseValue(const PropertyType& type); \
    PassRefPtr<TearOffType> LowerProperty(); \
\
private: \
    void synchronize##UpperProperty(); \
    static PassRefPtr<SVGAnimatedProperty> lookupOrCreate##UpperProperty##Wrapper(SVGElement* maskedOwnerType); \
    static void synchronize##UpperProperty(SVGElement* maskedOwnerType); \
\
    mutable SVGSynchronizableAnimatedProperty<PropertyType> m_##LowerProperty;

#define END_DECLARE_ANIMATED_PROPERTIES \
    CleanUpAnimatedPropertiesCaller m_cleanupAnimatedPropertiesCaller;

// List specific definition/declaration helpers
#define DECLARE_ANIMATED_LIST_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty) \
DECLARE_ANIMATED_PROPERTY(TearOffType, PropertyType, UpperProperty, LowerProperty) \
void detachAnimated##UpperProperty##ListWrappers(unsigned newListSize) \
{ \
    if (TearOffType* wrapper = SVGAnimatedProperty::lookupWrapper<UseOwnerType, TearOffType>(this, LowerProperty##PropertyInfo())) \
        wrapper->detachListWrappers(newListSize); \
}

}

#endif // SVGAnimatedPropertyMacros_h
