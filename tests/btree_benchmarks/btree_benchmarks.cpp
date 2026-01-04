/*
 * Copyright (c) 2026 Guillermo Hernan Martin
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

#include "bmap.h"

using namespace coll;

// Estructura para almacenar resultados
struct BenchmarkResult
{
    std::string map_name;
    size_t map_size;
    std::string operation;
    double duration_ms;
};

// Adaptador genérico para búsqueda (devuelve bool)
template <typename Map, typename Key>
inline bool map_find(const Map& m, const Key& key)
{
    return m.find(key) != m.end();
}

// Especialización para bmap: find devuelve Handle, convertimos a bool con has_value()
template <typename Key, typename Value, size_t Order>
inline bool map_find(const bmap<Key, Value, Order>& m, const Key& key)
{
    return m.find(key).has_value();
}

// Adaptador genérico para inserción
template <typename Map, typename Key, typename Value>
inline void map_insert(Map& m, const Key& key, const Value& value)
{
    m.insert({key, value});
}

// Especialización para bmap (insert overload es distinta)
template <typename Key, typename Value, size_t Order>
inline void map_insert(bmap<Key, Value, Order>& m, const Key& key, const Value& value)
{
    m.insert(key, value);
}

// Adaptador genérico para borrado (erase)
template <typename Map, typename Key>
inline bool map_erase(Map& m, const Key& key)
{
    return m.erase(key) > 0;
}

// Especialización para bmap (erase devuelve bool directamente)
template <typename Key, typename Value, size_t Order>
inline bool map_erase(bmap<Key, Value, Order>& m, const Key& key)
{
    return m.erase(key);
}

// Adaptador para std::map (pair<const Key, Value>)
template <typename Pair>
inline auto get_value(const Pair& p) -> decltype(p.second)
{
    return p.second;
}

// Adaptador para bmap::Range o Entry (estructura que tiene métodos key() y value())
template <typename Entry>
inline auto get_value(const Entry& e) -> decltype(e.value)
{
    return e.value;
}

template <typename MapType, typename Key, typename Value>
BenchmarkResult run_insertion(size_t n, const std::string& name)
{
    MapType m;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i)
        map_insert(m, static_cast<Key>(i), static_cast<Value>(i));
    auto end = std::chrono::high_resolution_clock::now();
    return {name, n, "insertion", std::chrono::duration<double, std::milli>(end - start).count()};
}

template <typename MapType, typename Key, typename Value>
BenchmarkResult run_insertion_random(size_t n, const std::string& name)
{
    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<Key> dist(0, static_cast<Key>(n * 10));
    std::vector<Key> keys(n);
    for (size_t i = 0; i < n; ++i)
        keys[i] = dist(rng);

    MapType m;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i)
        map_insert(m, keys[i], static_cast<Value>(i));
    auto end = std::chrono::high_resolution_clock::now();
    return {name, n, "insertion_random", std::chrono::duration<double, std::milli>(end - start).count()};
}

template <typename MapType, typename Key>
BenchmarkResult run_find(size_t n, const std::string& name)
{
    MapType m;
    for (size_t i = 0; i < n; ++i)
        map_insert(m, static_cast<Key>(i), static_cast<Key>(i));

    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<Key> dist(0, static_cast<Key>(n - 1));
    size_t found = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i)
    {
        Key key = dist(rng);
        if (map_find(m, key))
        {
            ++found;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    // Usar volatile found para evitar optimizaciones agresivas, aunque no requerido aquí
    volatile auto dummy = found;

    return {name, n, "find", std::chrono::duration<double, std::milli>(end - start).count()};
}

template <typename MapType, typename Key>
BenchmarkResult run_erase(size_t n, const std::string& name)
{
    MapType m;
    for (size_t i = 0; i < n; ++i)
        map_insert(m, static_cast<Key>(i), static_cast<Key>(i));

    size_t erased = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < n; ++i)
    {
        if (map_erase(m, static_cast<Key>(i)))
            ++erased;
    }
    auto end = std::chrono::high_resolution_clock::now();
    return {name, n, "erase", std::chrono::duration<double, std::milli>(end - start).count()};
}

template <typename MapType>
BenchmarkResult run_seq_read(size_t n, const std::string& name)
{
    MapType m;
    for (size_t i = 0; i < n; ++i)
    {
        map_insert(
            m,
            static_cast<typename MapType::key_type>(i),
            static_cast<typename MapType::mapped_type>(i)
        );
    }

    volatile typename MapType::mapped_type sink {};
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& kv : m)
        sink = get_value(kv);
    auto end = std::chrono::high_resolution_clock::now();
    return {name, n, "sequential_read", std::chrono::duration<double, std::milli>(end - start).count()};
}

// Función variádica para ejecutar todas las pruebas para todos los mapas pasados y acumular resultados
template <typename... Maps>
std::vector<BenchmarkResult>
run_all_benchmarks_for_size(size_t n, const std::vector<std::string>& map_names, Maps&&... maps)
{
    std::vector<BenchmarkResult> results;

    auto run_for_map = [&](auto&& map_type, const std::string& name)
    {
        results.push_back(run_insertion<std::decay_t<decltype(map_type)>, int, int>(n, name));
        results.push_back(run_insertion_random<std::decay_t<decltype(map_type)>, int, int>(n, name));
        results.push_back(run_find<std::decay_t<decltype(map_type)>, int>(n, name));
        results.push_back(run_erase<std::decay_t<decltype(map_type)>, int>(n, name));
        results.push_back(run_seq_read<std::decay_t<decltype(map_type)>>(n, name));
    };

    size_t idx = 0;
    (run_for_map(std::forward<Maps>(maps), map_names[idx++]), ...);

    return results;
}

// Imprime tablas con filas para cada mapa y columnas para cada tamaño, filtrando por operación
void print_results_table(
    const std::vector<BenchmarkResult>& results,
    const std::string& operation,
    const std::vector<std::string>& map_names,
    const std::vector<size_t>& map_sizes
)
{
    std::cout << "\n== Resultados para operación: " << operation << " ==\n";

    std::cout << std::setw(25) << "Configuración";
    for (auto size : map_sizes)
        std::cout << std::setw(15) << size;
    std::cout << '\n';

    for (const auto& map_name : map_names)
    {
        std::cout << std::setw(25) << std::setprecision(4) << map_name;
        for (auto size : map_sizes)
        {
            auto it = std::find_if(
                results.begin(),
                results.end(),
                [&](const BenchmarkResult& r)
                { return r.map_name == map_name && r.map_size == size && r.operation == operation; }
            );
            if (it != results.end())
                std::cout << std::setw(15) << it->duration_ms;
            else
                std::cout << std::setw(15) << "-";
        }
        std::cout << '\n';
    }
}

int main()
{
    std::vector<size_t> map_sizes = {10, 100, 1000, 10000, 100000, 1000000};
    std::vector<std::string> map_names {
        "std::map",
        "std::unordered_map",
        "bmap order 4",
        "bmap order 16",
        "bmap order 32",
        "bmap order 64",
        "bmap order 256"
    };

    using StdMap = std::map<int, int>;
    using StdUnordered = std::unordered_map<int, int>;
    using BTree4 = bmap<int, int, 4>;
    using BTree16 = bmap<int, int, 16>;
    using BTree32 = bmap<int, int, 32>;
    using BTree64 = bmap<int, int, 64>;
    using BTree256 = bmap<int, int, 256>;

    std::vector<BenchmarkResult> all_results;

    for (auto n : map_sizes)
    {
        std::cout << "Running tests for size: " << n << " ...\n";

        auto results = run_all_benchmarks_for_size(
            n,
            map_names,
            StdMap {},
            StdUnordered {},
            BTree4 {},
            BTree16 {},
            BTree32 {},
            BTree64 {},
            BTree256 {}
        );
        all_results.insert(all_results.end(), results.begin(), results.end());
    }

    for (auto& op : {"insertion", "insertion_random", "find", "erase", "sequential_read"})
    {
        print_results_table(all_results, op, map_names, map_sizes);
    }

    return 0;
}