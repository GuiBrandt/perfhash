/**
 * @file perfhash.hpp
 * @author Guilherme Brandt (gui.g.brandt@gmail.com)
 * @brief Header-only perfect hash container class for C++
 * @version 0.1
 * @date 2020-05-29
 * 
 * @copyright Copyright (c) 2020
 * 
 */

#include <memory>
#include <random>
#include <vector>
#include <list>
#include <type_traits>

namespace hashing {
    using hash_t = size_t;

    inline static unsigned int
    log2(size_t value)
    {
        unsigned int log;
        for (log = 0; value != 0; log++) value >>= 1;
        return log;
    }

    /**
     * @brief Standard randomized universal hash functor.
     * 
     * @tparam Key type of key to be hashed.
     */
    template <
        typename Key,
        template <typename>
        typename RandomDistribution = std::uniform_int_distribution,
        typename RandomDevice = std::random_device,
        typename RNG = std::mt19937,
        typename Enable = void
    > struct ru_hash_function {
        using random_device_t = RandomDevice;

        RNG rng;
        size_t M;

        hash_t operator()(const Key& key) const;
    };

    /**
     * @brief Standard randomized universal hash functor for integers.
     * 
     * @tparam Integer type of integral number to be hashed.
     */
    template <
        typename Integer,
        template <typename> typename RandomDistribution,
        typename RandomDevice,
        typename RNG
    > struct ru_hash_function <
        Integer,
        RandomDistribution,
        RandomDevice,
        RNG,
        typename std::enable_if_t<std::is_integral_v<Integer>>
    > {
        using random_device_t = RandomDevice;
        static constexpr size_t w = sizeof(Integer) * 8;

        RNG rng;
        size_t M;
        RandomDistribution<Integer> rdist;
        Integer a, b;

        void
        seed(RandomDevice& rd) {
            rng.seed(rd());
            rehash();
        }

        void
        rehash() {
            a = rdist(rng);
            b = rdist(rng) >> M;
        }

        hash_t
        operator()(const Integer& key) const {
            return unsigned(a * key + b) >> (w - M);
        }
    };

    /**
     * @brief Static collision-free hash map.
     * 
     * Implemented using FKS two-tiered hashing.
     * 
     * @tparam Key type of key used in the map.
     * @tparam Value type of value stored in the map.
     * @tparam RandomizedUniversalHash randomized universal hashing function.
     */
    template <
        typename Key,
        typename Value,
        typename RUHashFunction = ru_hash_function<Key>,
        typename Allocator = std::allocator<std::pair<const Key, Value>>,
        std::enable_if_t<std::is_copy_constructible_v<Key>, int> = 0,
        std::enable_if_t<std::is_default_constructible_v<Value>, int> = 0,
        std::enable_if_t<std::is_copy_constructible_v<Value>, int> = 0
    > class perfect_hash_map {
    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<const key_type, mapped_type>;
        using size_type = size_t;
        using allocator_type = Allocator;
        using reference = Value&;
        using const_reference = const Value&;
        using pointer = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
        using hash_function = RUHashFunction;

        perfect_hash_map() = delete;
        perfect_hash_map(const perfect_hash_map&) = default;
        perfect_hash_map(perfect_hash_map&&) = default;

        template <typename Iterator>
        perfect_hash_map(Iterator first, Iterator last,
                         const allocator_type& allocator = allocator_type()) {
            m_hash.M = log2(std::distance(first, last));
            m_hash.seed(m_random_device);
            m_buckets.resize(1ULL << m_hash.M,
                SubHash(m_random_device, allocator));
            populate(first, last);
        }

        perfect_hash_map(const std::initializer_list<value_type>& values,
                         const allocator_type& allocator = allocator_type())
                         : this(values.begin(), values.end(), allocator) {};
        
        perfect_hash_map& operator=(const perfect_hash_map&) = default;
        perfect_hash_map& operator=(perfect_hash_map&&) = default;

        ~perfect_hash_map() = default;
        
        const_reference at(const key_type& key) const {
            return m_buckets[m_hash(key)].at(key);
        }

        reference at(const key_type& key) {
            return m_buckets[m_hash(key)].at(key);
        }

        const_reference operator[](const key_type& key) const {
            return m_buckets[m_hash(key)][key];
        }

        reference operator[](const key_type& key) {
            return m_buckets[m_hash(key)][key];
        }
    private:
        using random_device_t = typename hash_function::random_device_t;

        struct SubHash {
            hash_function hash;
            std::vector<std::pair<key_type, mapped_type>, allocator_type> buckets;

            SubHash(random_device_t& random_device,
                    const allocator_type& allocator)
                : buckets(allocator) {
                hash.seed(random_device);
            }

            void resize(size_t size) {
                size_t M = log2(size);
                hash.M = M;
                buckets.resize(1ULL << M);
            }

            void rehash() {
                hash.rehash();
            }

            void clear() {
                buckets.clear();
            }
            
            void add(const value_type& pair) {
                buckets[hash(pair.first)] = pair;
            }

            const_reference at(const key_type& key) const {
                decltype(buckets)::const_reference pair = buckets[hash(key)];
                if (pair.first != key) throw std::out_of_range("No such key");
                return pair.second;
            }

            reference at(const key_type& key) {
                decltype(buckets)::reference pair = buckets[hash(key)];
                if (pair.first != key) throw std::out_of_range("No such key");
                return pair.second;
            }
            
            const_reference operator[](const key_type& key) const {
                return buckets[hash(key)].second;
            }

            reference operator[](const key_type& key) {
                return buckets[hash(key)].second;
            }

            size_t capacity() const {
                return buckets.size();
            }
        };
        
        hash_function m_hash;
        random_device_t m_random_device;
        std::vector<SubHash> m_buckets;
        std::list<key_type> m_keys;

        template <typename Iterator>
        void populate(const Iterator& first, const Iterator& last) {
            std::vector<std::vector<value_type>> hashed(m_buckets.size());
            for (auto it = first; it != last; it++) {
                m_keys.push_back(it->first);
                hashed.at(m_hash(it->first)).push_back(*it);
            }

            for (size_t i = 0; i < m_buckets.size(); i++) {
                auto& bucket = m_buckets.at(i);
                auto& elements = hashed.at(i);
                if (elements.empty()) continue;

                size_t l = elements.size();
                bucket.resize(l * l);
                do_perfect(bucket, elements);
            }
        }

        void do_perfect(SubHash& bucket,
                   const std::vector<value_type>& elements) {
            bool collision;
            std::vector<bool> dummy(bucket.capacity());
            do {
                collision = false;
                for (auto& e : elements) {
                    hash_t h = bucket.hash(e.first);
                    if (dummy.at(h)) {
                        collision = true;
                        dummy.clear();
                        bucket.rehash();
                        break;
                    } else {
                        dummy.at(h) = true;
                    }
                }
            } while (collision);
            for (auto& e : elements) bucket.add(e);
        }
    };
};
