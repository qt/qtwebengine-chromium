/**
 * @license
 * Copyright 2023 Google Inc.
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @internal
 */
export declare function unpackArchive(archivePath: string, folderPath: string): Promise<void>;
/**
 * @internal
 */
export declare const internalConstantsForTesting: {
    xz: string;
    bzip2: string;
};
/**
 * @internal
 */
export declare function extractZipWithYauzl(archivePath: string, folderPath: string): Promise<void>;
//# sourceMappingURL=fileUtil.d.ts.map