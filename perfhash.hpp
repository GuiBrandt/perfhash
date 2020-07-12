/**
 * @file perfhash.hpp
 * @author Guilherme Brandt (gui.g.brandt@gmail.com)
 * @brief Header-only perfect hash container class for C++
 * @version 0.1
 * @date 2020-05-29
 * 
 * @copyright Copyright (c) 2020
 */

#include <memory>
#include <random>
#include <vector>
#include <list>
#include <type_traits>
#include <cassert>

namespace perfhash {
    using hash_t = size_t;

    inline static unsigned int log2(size_t value) {
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
        void seed(RandomDevice& rd);
        void rehash();
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

        void seed(RandomDevice& rd) {
            rng.seed(rd());
            rehash();
        }

        void rehash() {
            a = rdist(rng);
            b = rdist(rng) >> M;
        }

        hash_t operator()(const Integer& key) const {
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
        typename Allocator = std::allocator<std::pair<Key, Value>>,
        std::enable_if_t<std::is_copy_constructible_v<Key>, int> = 0,
        std::enable_if_t<std::is_default_constructible_v<Value>, int> = 0,
        std::enable_if_t<std::is_copy_constructible_v<Value>, int> = 0
    > class perfect_hash_map {
    public:
        using key_type = Key;
        using mapped_type = Value;
        using value_type = std::pair<key_type, mapped_type>;
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

        /**
         * @brief Range constructor.
         * 
         * Constructs a container with as many elements as the range
         * [first,last), with each element constructed from its corresponding
         * element in that range.
         * 
         * @tparam Iterator STL iterator for a collection type.
         * @param first Iterator to the initial position in a range.
         * @param last Iterator to the final position in a range.
         * @param allocator Allocator object.
         */
        template <typename Iterator>
        perfect_hash_map(Iterator first, Iterator last,
                         const allocator_type& allocator = allocator_type()) {
            m_hash.M = log2(std::distance(first, last));
            m_hash.seed(m_random_device);
            m_buckets.resize(1ULL << m_hash.M,
                SubHash(m_random_device, allocator));
            populate(first, last);
        }

        /**
         * @brief Initializer list constructor.
         * 
         * @param values An initializer_list object.
         * @param allocator Allocator object.
         */
        perfect_hash_map(const std::initializer_list<value_type>& values,
                         const allocator_type& allocator = allocator_type()) {
            this(values.begin(), values.end(), allocator);
        }
        
        /**
         * @brief Copy assignment operator.
         * 
         * @param other An object of the same type.
         * @return perfect_hash_map& *this.
         */
        perfect_hash_map& operator=(const perfect_hash_map& other) = default;
        
        /**
         * @brief Move assignment operator.
         * 
         * @param other An object of the same type.
         * @return perfect_hash_map& *this.
         */
        perfect_hash_map& operator=(perfect_hash_map&& other) = default;

        ~perfect_hash_map() = default;

        /**
         * @brief Safe access element.
         * 
         * @param key Key for the element.
         * @return const_reference A reference to the mapped value.
         * 
         * @throw std::out_of_range If there's no matching key in the
         *  container.
         * @see operator[]
         */
        const_reference at(const key_type& key) const {
            hash_t h = m_hash(key);
            assert(h < m_buckets.size());
            return m_buckets[h].at(key);
        }

        reference at(const key_type& key) {
            hash_t h = m_hash(key);
            assert(h < m_buckets.size());
            return m_buckets[h].at(key);
        }

        /**
         * @brief Access element.
         * 
         * @param key Key for the element.
         * @return const_reference A reference to the mapped value.
         * 
         * @note If the container has no matching key, this operator has
         *  undefined behavior.
         * @see at
         */
        const_reference operator[](const key_type& key) const {
            hash_t h = m_hash(key);
            assert(h < m_buckets.size());
            return m_buckets[h][key];
        }

        reference operator[](const key_type& key) {
            hash_t h = m_hash(key);
            assert(h < m_buckets.size());
            return m_buckets[h][key];
        }
    private:
        using random_device_t = typename hash_function::random_device_t;

        struct SubHash {
            hash_function hash;
            std::vector<value_type, allocator_type> buckets;

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
                hash_t h = hash(key);
                if (h >= buckets.size()) throw std::out_of_range("No such key");

                const value_type& pair = buckets[h];
                if (pair.first != key) throw std::out_of_range("No such key");
                return pair.second;
            }

            reference at(const key_type& key) {
                hash_t h = hash(key);
                if (h >= buckets.size()) throw std::out_of_range("No such key");

                value_type& pair = buckets[h];
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
                hash_t h = m_hash(it->first);
                assert(h < hashed.size());
                hashed[h].push_back(*it);
            }

            for (size_t i = 0; i < m_buckets.size(); i++) {
                auto& bucket = m_buckets[i];
                auto& elements = hashed[i];
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
                    assert(h < dummy.size());
                    if (dummy[h]) {
                        collision = true;
                        std::fill(dummy.begin(), dummy.end(), false);
                        bucket.rehash();
                        break;
                    } else {
                        dummy[h] = true;
                    }
                }
            } while (collision);
            for (auto& e : elements) bucket.add(e);
        }
    };
};
