/*
 * =============================================================================
 * src/olnk/hash.cpp
 * =============================================================================
 *
 * Internal hashing utilities for olnk.
 *
 * Why this file exists:
 *   - The linker needs fast, stable hashing in many places:
 *       * symbol table keys
 *       * path / file identity keys
 *       * plugin and format option lookup
 *       * incremental build cache keys
 *       * content fingerprints for sections / objects / outputs
 *
 * Design choices:
 *   - We use xxHash because it is already a project dependency and offers
 *     excellent speed/quality tradeoffs for non-cryptographic hashing.
 *   - This layer hides the exact backend from the rest of the codebase.
 *   - We prefer explicit 64-bit hashing as the default because many linker
 *     identities are naturally represented as 64-bit values and because stable
 *     64-bit hashes are generally sufficient for internal dedup/indexing.
 *
 * Important notes:
 *   - This is NOT cryptographic hashing.
 *   - Do not use these hashes as security boundaries.
 *   - If these hashes are used for persistent incremental state, then the
 *     algorithm + seed become part of on-disk compatibility and must not be
 *     changed casually.
 *
 * LLM AGENT HINTS:
 *   - If you change the default seed, hashing algorithm, or byte ordering of
 *     compound values, you may silently break:
 *       * incremental cache reuse
 *       * symbol table determinism
 *       * serialized fingerprints
 *   - Prefer adding new APIs rather than changing semantics of existing ones.
 *   - If later you need cryptographic verification, create a separate module
 *     instead of extending this one.
 *   - If the project later introduces a public/internal header for hashing,
 *     keep this file as the implementation backend and avoid duplicating hash
 *     logic across translation units.
 * =============================================================================
 */

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <xxhash.h>

namespace olnk {

/*
 * ---------------------------------------------------------------------------
 * Internal hash typedef
 * ---------------------------------------------------------------------------
 *
 * LLM AGENT HINT:
 *   - If the project later introduces a canonical internal type alias in a
 *     header, remove this local alias and use the shared one instead.
 *   - Keep the underlying type fixed-width if hashes are persisted or exposed
 *     between modules.
 */
using hash64_t = std::uint64_t;

/*
 * ---------------------------------------------------------------------------
 * Default seed
 * ---------------------------------------------------------------------------
 *
 * This seed is intentionally fixed to make results deterministic.
 *
 * LLM AGENT HINTS:
 *   - Treat this value as part of the hash ABI if hashes are persisted.
 *   - Do not randomize per-process unless you are intentionally introducing
 *     non-deterministic hash hardening for in-memory tables only.
 *   - If you need both deterministic and randomized hashing, create separate
 *     APIs or separate seed domains.
 */
static constexpr hash64_t kDefaultSeed = 0x9E3779B185EBCA87ULL;

/*
 * ---------------------------------------------------------------------------
 * hash_bytes64
 * ---------------------------------------------------------------------------
 *
 * Hash an arbitrary byte range into a stable 64-bit value.
 *
 * Semantics:
 *   - nullptr + size == 0 is treated as empty input.
 *   - nullptr + size > 0 is also treated defensively as empty input rather
 *     than dereferencing invalid memory. This avoids crashes in leaf helpers,
 *     though callers should still pass valid buffers.
 *
 * LLM AGENT HINTS:
 *   - If the rest of the codebase distinguishes between "null" and "empty",
 *     add separate APIs instead of changing this one.
 *   - If stronger input validation is desired, do it in debug assertions or
 *     at higher layers; do not silently change hash semantics for empty input.
 *   - Keep this function noexcept and allocation-free.
 */
hash64_t hash_bytes64(const void* data,
                      std::size_t size,
                      hash64_t seed = kDefaultSeed) noexcept {
    if (data == nullptr || size == 0) {
        return static_cast<hash64_t>(
            XXH3_64bits_withSeed("", 0, static_cast<XXH64_hash_t>(seed)));
    }

    return static_cast<hash64_t>(
        XXH3_64bits_withSeed(data, size, static_cast<XXH64_hash_t>(seed)));
}

/*
 * ---------------------------------------------------------------------------
 * hash_string64
 * ---------------------------------------------------------------------------
 *
 * Hash a string view as raw bytes.
 *
 * LLM AGENT HINTS:
 *   - This hashes bytes exactly as provided; it does NOT normalize:
 *       * path separators
 *       * case
 *       * Unicode normalization
 *       * trailing slashes
 *   - If path hashing later needs canonicalization, do that in a dedicated
 *     helper before calling this function.
 *   - std::string_view is safe here because the function does not retain data.
 */
hash64_t hash_string64(std::string_view s,
                       hash64_t seed = kDefaultSeed) noexcept {
    return hash_bytes64(s.data(), s.size(), seed);
}

/*
 * ---------------------------------------------------------------------------
 * hash_u64_pair
 * ---------------------------------------------------------------------------
 *
 * Hash two 64-bit integers as a compound value.
 *
 * We intentionally feed the two values as a packed byte sequence to xxHash,
 * rather than inventing a custom mixer. This keeps behavior simple and stable.
 *
 * LLM AGENT HINTS:
 *   - The byte representation of this struct is part of the function’s
 *     semantics. Keep field order stable.
 *   - This is fine for same-build/platform determinism; if you need
 *     cross-endianness canonicalization for persisted data, serialize values in
 *     a fixed byte order explicitly before hashing.
 *   - Do not insert padding-sensitive fields here unless layout is controlled.
 */
hash64_t hash_u64_pair(hash64_t a,
                       hash64_t b,
                       hash64_t seed = kDefaultSeed) noexcept {
    struct pair_bytes_t {
        std::uint64_t first;
        std::uint64_t second;
    } pair{a, b};

    return hash_bytes64(&pair, sizeof(pair), seed);
}

/*
 * ---------------------------------------------------------------------------
 * hash_combine64
 * ---------------------------------------------------------------------------
 *
 * Combine an existing hash with another 64-bit value.
 *
 * This is intentionally simple: compound combination is delegated to the same
 * primitive used elsewhere.
 *
 * LLM AGENT HINTS:
 *   - If later the project introduces domain-separated combination
 *     (e.g. different seeds for symbols/paths/sections), keep this helper as a
 *     generic primitive and layer domain-specific helpers on top.
 *   - Avoid changing combination ordering semantics; hash_combine64(a, b)
 *     should not become equivalent to hash_combine64(b, a) unless that is
 *     explicitly intended everywhere.
 */
hash64_t hash_combine64(hash64_t current,
                        hash64_t next,
                        hash64_t seed = kDefaultSeed) noexcept {
    return hash_u64_pair(current, next, seed);
}

/*
 * ---------------------------------------------------------------------------
 * Streaming / incremental hash builder
 * ---------------------------------------------------------------------------
 *
 * This class is useful when callers want to hash multiple chunks without
 * concatenating them into a temporary buffer.
 *
 * Example uses:
 *   - hashing file contents read in blocks
 *   - hashing structured state incrementally
 *   - generating fingerprints from multiple independent fields
 *
 * LLM AGENT HINTS:
 *   - Keep this class move-only unless there is a strong reason to clone state.
 *   - Avoid exceptions; this file is suitable for low-level runtime use.
 *   - If XXH3 state allocation ever becomes a problem, a stack-based or pooled
 *     strategy could be introduced, but only after measuring.
 *   - If you later add "update(uint64_t)" helpers, define the byte order
 *     explicitly if hashes are persisted across platforms.
 */
class hash64_builder {
public:
    explicit hash64_builder(hash64_t seed = kDefaultSeed) noexcept
        : seed_(seed) {
        state_ = XXH3_createState();
        if (state_ != nullptr) {
            (void)XXH3_64bits_reset_withSeed(
                state_, static_cast<XXH64_hash_t>(seed_));
        }
    }

    ~hash64_builder() {
        if (state_ != nullptr) {
            XXH3_freeState(state_);
            state_ = nullptr;
        }
    }

    hash64_builder(const hash64_builder&) = delete;
    hash64_builder& operator=(const hash64_builder&) = delete;

    hash64_builder(hash64_builder&& other) noexcept
        : state_(other.state_), seed_(other.seed_) {
        other.state_ = nullptr;
        other.seed_ = 0;
    }

    hash64_builder& operator=(hash64_builder&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        if (state_ != nullptr) {
            XXH3_freeState(state_);
        }

        state_ = other.state_;
        seed_ = other.seed_;

        other.state_ = nullptr;
        other.seed_ = 0;

        return *this;
    }

    /*
     * Feed raw bytes into the running hash.
     *
     * LLM AGENT HINT:
     *   - Null or empty updates are ignored.
     *   - If state allocation failed, updates become no-ops and digest() falls
     *     back to a deterministic empty hash. This is intentionally defensive
     *     for low-level utility code.
     */
    void update(const void* data, std::size_t size) noexcept {
        if (state_ == nullptr || data == nullptr || size == 0) {
            return;
        }

        (void)XXH3_64bits_update(state_, data, size);
    }

    /*
     * Feed string bytes into the running hash.
     */
    void update(std::string_view s) noexcept {
        update(s.data(), s.size());
    }

    /*
     * Final digest of the current state.
     *
     * LLM AGENT HINTS:
     *   - Calling digest() does not reset the state.
     *   - Repeated digest() calls without update() should return the same value.
     *   - If a one-shot finalize-and-reset API is needed later, add it
     *     explicitly rather than changing digest() behavior.
     */
    hash64_t digest() const noexcept {
        if (state_ == nullptr) {
            return hash_bytes64("", 0, seed_);
        }

        return static_cast<hash64_t>(XXH3_64bits_digest(state_));
    }

    /*
     * Reset builder to the original seed.
     *
     * LLM AGENT HINT:
     *   - Reset preserves the seed domain used at construction time.
     *   - If callers need reseeding, add a dedicated reseed() API.
     */
    void reset() noexcept {
        if (state_ == nullptr) {
            return;
        }

        (void)XXH3_64bits_reset_withSeed(
            state_, static_cast<XXH64_hash_t>(seed_));
    }

    /*
     * Expose seed for debugging/introspection if needed by callers later.
     *
     * LLM AGENT HINT:
     *   - Keeping this accessor cheap and trivial is useful for tests.
     */
    hash64_t seed() const noexcept {
        return seed_;
    }

private:
    XXH3_state_t* state_ = nullptr;
    hash64_t seed_ = kDefaultSeed;
};

/*
 * =============================================================================
 * LLM AGENT NOTES FOR FUTURE EXTENSIONS
 * =============================================================================
 *
 * Good next additions:
 *   - domain-separated helpers:
 *       * hash_path64(...)
 *       * hash_symbol_name64(...)
 *       * hash_section_identity64(...)
 *   - canonical serialization helpers for structured hashing
 *   - file-content hashing helpers in file_io.cpp or a shared utility layer
 *
 * Things to be careful about:
 *   - Persisted hashes vs ephemeral in-memory hashes are different problems.
 *   - Cross-platform reproducibility may require fixed-endian encoding of
 *     integers before hashing.
 *   - If this file becomes widely depended on, keep compile-time overhead low
 *     and avoid dragging in heavy project headers.
 * =============================================================================
 */

} // namespace olnk
