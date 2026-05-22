#ifndef OLNK_OLNK_VERSION_H
#define OLNK_OLNK_VERSION_H

/*
 * =============================================================================
 * LLM AGENT NOTES / PATCH GUIDANCE
 * =============================================================================
 *
 * Purpose of this file:
 *   - Provide a tiny, dependency-free version interface for olnk.
 *   - Safe to include from both C and C++.
 *   - Should remain stable because external plugins/tools may rely on it.
 *
 * Improvement opportunities for future patches:
 *   1. Sync these version macros with CMake/project() automatically.
 *   2. Add ABI versioning separate from semantic versioning if needed.
 *   3. Expose build metadata (git commit, dirty state, compiler, target).
 *   4. Add a packed integer version helper if the CLI/API needs comparisons.
 *   5. If plugin ABI becomes strict, introduce:
 *        - OLNK_PLUGIN_ABI_VERSION
 *        - OLNK_FORMAT_ABI_VERSION
 *        - OLNK_MACHINE_ABI_VERSION
 *
 * Constraints:
 *   - Keep this file lightweight and standalone.
 *   - Avoid including STL or any third-party headers here.
 *   - Preserve C compatibility.
 *
 * If patching:
 *   - Prefer additive changes.
 *   - Do not break existing macro names without aliases/deprecation path.
 * =============================================================================
 */

#define OLNK_VERSION_MAJOR 0
#define OLNK_VERSION_MINOR 1
#define OLNK_VERSION_PATCH 0

#define OLNK_VERSION_STRING "0.1.0"

/*
 * Numeric version helper:
 *   0xMMmmpp -> major/minor/patch packed into one integer.
 * Useful for preprocessor comparisons.
 */
#define OLNK_VERSION_HEX \
    (((OLNK_VERSION_MAJOR & 0xFF) << 16) | \
     ((OLNK_VERSION_MINOR & 0xFF) << 8)  | \
     ((OLNK_VERSION_PATCH & 0xFF) << 0))

/*
 * Public API version.
 * This should be incremented when the exported C-facing API changes in a way
 * that external consumers may need to detect.
 */
#define OLNK_API_VERSION 1

/*
 * Stringification helpers.
 */
#define OLNK_DETAIL_STRINGIFY_IMPL(x) #x
#define OLNK_DETAIL_STRINGIFY(x) OLNK_DETAIL_STRINGIFY_IMPL(x)

/*
 * Human-friendly full version string.
 * Can be useful in diagnostics, banners, or plugin negotiation.
 */
#define OLNK_VERSION_STRING_FULL \
    OLNK_DETAIL_STRINGIFY(OLNK_VERSION_MAJOR) "." \
    OLNK_DETAIL_STRINGIFY(OLNK_VERSION_MINOR) "." \
    OLNK_DETAIL_STRINGIFY(OLNK_VERSION_PATCH)

#endif /* OLNK_OLNK_VERSION_H */
