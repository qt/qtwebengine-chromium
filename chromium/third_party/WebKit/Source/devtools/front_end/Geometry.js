/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.Geometry = {};

/**
 * @type {number}
 */
WebInspector.Geometry._Eps = 1e-5;

/**
 * @constructor
 * @param {number} x
 * @param {number} y
 * @param {number} z
 */
WebInspector.Geometry.Vector = function(x, y, z)
{
    this.x = x;
    this.y = y;
    this.z = z;
}

WebInspector.Geometry.Vector.prototype = {
    /**
     * @return {number}
     */
    length: function()
    {
        return Math.sqrt(this.x * this.x + this.y * this.y + this.z * this.z);
    },

    normalize: function()
    {
        var length = this.length();
        if (length <= WebInspector.Geometry._Eps)
            return;

        this.x /= length;
        this.y /= length;
        this.z /= length;
    }
}

/**
 * @constructor
 * @param {number} alpha
 * @param {number} beta
 * @param {number} gamma
 */
WebInspector.Geometry.EulerAngles = function(alpha, beta, gamma)
{
    this.alpha = alpha;
    this.beta = beta;
    this.gamma = gamma;
}

/**
 * @param {!CSSMatrix} rotationMatrix
 * @return {!WebInspector.Geometry.EulerAngles}
 */
WebInspector.Geometry.EulerAngles.fromRotationMatrix = function(rotationMatrix)
{
    var beta = Math.atan2(rotationMatrix.m23, rotationMatrix.m33);
    var gamma = Math.atan2(-rotationMatrix.m13, Math.sqrt(rotationMatrix.m11 * rotationMatrix.m11 + rotationMatrix.m12 * rotationMatrix.m12));
    var alpha = Math.atan2(rotationMatrix.m12, rotationMatrix.m11);
    return new WebInspector.Geometry.EulerAngles(WebInspector.Geometry.radToDeg(alpha), WebInspector.Geometry.radToDeg(beta), WebInspector.Geometry.radToDeg(gamma));
}

/**
 * @param {!WebInspector.Geometry.Vector} u
 * @param {!WebInspector.Geometry.Vector} v
 * @return {number}
 */
WebInspector.Geometry.scalarProduct = function(u, v)
{
    return u.x * v.x + u.y * v.y + u.z * v.z;
}

/**
 * @param {!WebInspector.Geometry.Vector} u
 * @param {!WebInspector.Geometry.Vector} v
 * @return {!WebInspector.Geometry.Vector}
 */
WebInspector.Geometry.crossProduct = function(u, v)
{
    var x = u.y * v.z - u.z * v.y;
    var y = u.z * v.x - u.x * v.z;
    var z = u.x * v.y - u.y * v.x;
    return new WebInspector.Geometry.Vector(x, y, z);
}

/**
 * @param {!WebInspector.Geometry.Vector} u
 * @param {!WebInspector.Geometry.Vector} v
 * @return {number}
 */
WebInspector.Geometry.calculateAngle = function(u, v)
{
    var uLength = u.length();
    var vLength = v.length();
    if (uLength <= WebInspector.Geometry._Eps || vLength <= WebInspector.Geometry._Eps)
        return 0;
    var cos = WebInspector.Geometry.scalarProduct(u, v) / uLength / vLength;
    if (Math.abs(cos) > 1)
        return 0;
    return WebInspector.Geometry.radToDeg(Math.acos(cos));
}

/**
 * @param {number} rad
 * @return {number}
 */
WebInspector.Geometry.radToDeg = function(rad)
{
    return rad * 180 / Math.PI;
}

