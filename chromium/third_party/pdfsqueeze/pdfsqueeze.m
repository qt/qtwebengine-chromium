//
//  pdfsqueeze.m
//  
//  Takes PDFs or Adobe Illustrator files and removes extraneous information
//  and reduces them down to the Generic RGB Profile.
//  Often useful as part of a build process.
//  
//  Copyright 2009 Google Inc.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not
//  use this file except in compliance with the License.  You may obtain a copy
//  of the License at
// 
//  http://www.apache.org/licenses/LICENSE-2.0
// 
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
//  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
//  License for the specific language governing permissions and limitations under
//  the License.
//

#import <Foundation/Foundation.h>
#import <Quartz/Quartz.h>
#import <stdio.h>
#include "ApplyGenericRGB.h"

static QuartzFilter *GetGenericProfileFilter();

int main (int argc, const char * argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  if (argc != 3) {
    fprintf(stderr, "pdfsqueeze <inputFile> <outputFile>\n"
            "Reduces the size of pdf or Adobe Illustrator files by "
            "removing extraneous information and applying generic "
            "rgb profile.\nVersion 1.0\n");
    return 1;
  }
  
  
  NSString *inPath = [NSString stringWithUTF8String:argv[1]];
  NSString *outPath = [NSString stringWithUTF8String:argv[2]];
  NSURL *inURL = [NSURL fileURLWithPath:inPath];
  NSURL *outURL = [NSURL fileURLWithPath:outPath];
  PDFDocument *pdf = [[[PDFDocument alloc] initWithURL:inURL] autorelease];
  if (!pdf) {
    fprintf(stderr, "pdfsqueeze:0: error: Unable to open %s\n", [inPath UTF8String]);
    return 1;
  }
  NSDictionary *oldAttributes = [pdf documentAttributes];
  NSMutableDictionary *newAttributes = [NSMutableDictionary dictionary];
  NSDate *date = [oldAttributes objectForKey:PDFDocumentCreationDateAttribute];
  if (date) {
    [newAttributes setObject:date forKey:PDFDocumentCreationDateAttribute];
  }
  date = [oldAttributes objectForKey:PDFDocumentModificationDateAttribute];
  if (date) {
    [newAttributes setObject:date forKey:PDFDocumentModificationDateAttribute];
  }
  [pdf setDocumentAttributes:newAttributes];
  QuartzFilter *filter = GetGenericProfileFilter();
  NSDictionary *options = [NSDictionary dictionaryWithObject:filter 
                                                      forKey:@"QuartzFilter"];
  if (![pdf writeToURL:outURL withOptions:options]) {
    fprintf(stderr, "pdfsqueeze:0: error: Unable to write to %s\n", [outPath UTF8String]);
  }
  [pool drain];
  return 0;
}

QuartzFilter *GetGenericProfileFilter() {
  // Create up our filter from the filter data
  NSData *data = [NSData dataWithBytesNoCopy:ApplyGenericRGB_qfilter
                                      length:ApplyGenericRGB_qfilter_len
                                freeWhenDone:NO];
  NSDictionary *filterProps 
    = [NSPropertyListSerialization propertyListFromData:data
                                       mutabilityOption:NSPropertyListImmutable
                                                 format:NULL
                                       errorDescription:nil];
  QuartzFilter *filter = [QuartzFilter quartzFilterWithProperties:filterProps];
  return filter;
}
