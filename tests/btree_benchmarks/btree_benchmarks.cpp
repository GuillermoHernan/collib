#include <iostream>
#include <map>
#include <chrono>
#include <random>

#include "btree.h"

// Adaptador genérico para obtener begin
template <typename Map>
inline auto map_begin(const Map& m) {
    return m.begin();
}

// Adaptador genérico para obtener end
template <typename Map>
inline auto map_end(const Map& m) {
    return m.end();
}

// Adaptador genérico para búsqueda (devuelve bool)
template <typename Map, typename Key>
inline bool map_find(const Map& m, const Key& key) {
    return m.find(key) != m.end();
}

// Especialización para BTreeMap: find devuelve Handle, convertimos a bool con has_value()
template <typename Key, typename Value, size_t Order>
inline bool map_find(const BTreeMap<Key, Value, Order>& m, const Key& key) {
    return m.find(key).has_value();
}

// Adaptador genérico para inserción
template <typename Map, typename Key, typename Value>
inline void map_insert(Map& m, const Key& key, const Value& value) {
    m.insert({ key, value });
}

// Especialización para BTreeMap (insert overload es distinta)
template <typename Key, typename Value, size_t Order>
inline void map_insert(BTreeMap<Key, Value, Order>& m, const Key& key, const Value& value) {
    m.insert(key, value);
}

// Adaptador genérico para borrado (erase)
template <typename Map, typename Key>
inline bool map_erase(Map& m, const Key& key) {
    return m.erase(key) > 0;
}

// Especialización para BTreeMap (erase devuelve bool directamente)
template <typename Key, typename Value, size_t Order>
inline bool map_erase(BTreeMap<Key, Value, Order>& m, const Key& key) {
    return m.erase(key);
}

// Adaptador para std::map (pair<const Key, Value>)
template <typename Pair>
inline auto get_value(const Pair& p) -> decltype(p.second) {
    return p.second;
}

// Adaptador para BTreeMap::Range o Entry (estructura que tiene métodos key() y value())
template <typename Entry>
inline auto get_value(const Entry& e) -> decltype(e.value) {
    return e.value;
}
// Benchmark de inserción
template <typename Map, typename Key, typename Value>
void benchmark_insertion(size_t n, const char* map_name) {
    Map m;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        map_insert(m, static_cast<Key>(i), static_cast<Value>(i));
    }
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Benchmark inserción (" << map_name << ") - " << n << " elementos: "
        << duration << " ms" << std::endl;
}

// Benchmark inserción con claves aleatorias
template <typename Map, typename Key, typename Value>
void benchmark_insertion_random(size_t n, const char* map_name, uint64_t seed = 12345) {
    Map m;
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<Key> dist(0, static_cast<Key>(n * 10));

    std::vector<Key> keys(n);
    for (size_t i = 0; i < n; ++i) {
        keys[i] = dist(rng);
    }

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        map_insert(m, keys[i], static_cast<Value>(i));
    }
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(end - start).count();

    std::cout << "Benchmark inserción aleatoria (" << map_name << ") - " << n << " elementos: "
        << duration << " ms" << std::endl;
}

// Benchmark de búsqueda
template <typename Map, typename Key>
void benchmark_find(size_t n, const char* map_name) {
    Map m;
    // Insertamos previamente los elementos
    for (size_t i = 0; i < n; ++i) {
        map_insert(m, static_cast<Key>(i), static_cast<Key>(i));
    }

    size_t found_count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        if (map_find(m, static_cast<Key>(i))) {
            ++found_count;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Benchmark búsqueda (" << map_name << ") - " << n << " elementos, encontrados: " << found_count << " en "
        << duration << " ms" << std::endl;
}

// Benchmark de borrado
template <typename Map, typename Key>
void benchmark_erase(size_t n, const char* map_name) {
    Map m;
    // Insertamos previamente los elementos
    for (size_t i = 0; i < n; ++i) {
        map_insert(m, static_cast<Key>(i), static_cast<Key>(i));
    }

    size_t erased_count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i) {
        if (map_erase(m, static_cast<Key>(i))) {
            ++erased_count;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Benchmark borrado (" << map_name << ") - " << n << " elementos, borrados: " << erased_count << " en "
        << duration << " ms" << std::endl;
}

// Benchmark lectura secuencial usando range-based for (para std::map y BTreeMap)
template <typename Map>
void benchmark_sequential_read(size_t n, const char* map_name) {
    Map m;
    // Insertar previamente elementos secuenciales
    for (size_t i = 0; i < n; ++i) {
        map_insert(m, static_cast<typename Map::key_type>(i), static_cast<typename Map::mapped_type>(i));
    }

    volatile typename Map::mapped_type sink{}; // evitar optimizaciones agresivas

    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& kv : m) {
        sink = get_value(kv);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double duration = std::chrono::duration<double, std::milli>(end - start).count();
    std::cout << "Benchmark lectura secuencial (range-for) (" << map_name << ") - " << n << " elementos: "
        << duration << " ms" << std::endl;
}


int main() {
    constexpr size_t N = 1'000'000;

    std::cout << "Inicio benchmarks..." << std::endl;

    using TestStdMap = std::map<int, int>;
    using TestBtreeMap = BTreeMap<int, int, 16>;

    benchmark_insertion<TestStdMap, int, int>(N, "std::map (secuencial)");
    benchmark_insertion<TestBtreeMap, int, int>(N, "BTreeMap (secuencial)");
    benchmark_insertion_random<TestStdMap, int, int>(N, "std::map (aleatorio)");
    benchmark_insertion_random<TestBtreeMap, int, int>(N, "BTreeMap (aleatorio)");
    benchmark_find<TestStdMap, int>(N, "std::map");
    benchmark_find<TestBtreeMap, int>(N, "BTreeMap");
    benchmark_erase<TestStdMap, int>(N, "std::map");
    benchmark_erase<TestBtreeMap, int>(N, "BTreeMap");
    benchmark_sequential_read<TestStdMap>(N, "std::map");
    benchmark_sequential_read<TestBtreeMap>(N, "BTreeMap");

    std::cout << "Benchmarks completados." << std::endl;

    return 0;
}
