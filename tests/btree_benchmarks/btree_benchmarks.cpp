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

#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

#include "bmap.h"

using namespace coll;

struct TestConfig
{
    std::string map_name;
    std::string operation;
    size_t map_size;
    size_t op_count;

    auto operator<=>(const TestConfig&) const = default;
};

// Estructura para almacenar resultados
struct BenchmarkResult
{
    TestConfig config;
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

class TestBase
{
public:
    void setup(const TestConfig& config) { m_config = config; }
    void pre_run() { }

protected:
    TestConfig m_config;
};

template <typename MapType>
class InsertionTest : public TestBase
{
public:
    void pre_run()
    {
        m_reps = m_config.op_count / m_config.map_size;
        m_reps = std::max(size_t(1), m_reps);
    }

    void run()
    {
        using Key = MapType::key_type;
        using Value = MapType::mapped_type;
        const size_t n = m_config.map_size;

        MapType m;
        for (size_t j = 0; j < m_reps; ++j)
        {
            for (size_t i = 0; i < n; ++i)
                map_insert(m, static_cast<Key>(i), static_cast<Value>(i));

            m.clear();
        }
    }

private:
    size_t m_reps = 1;
};

template <typename MapType>
class RandomInsertionTest : public TestBase
{
public:
    using Key = MapType::key_type;
    using Value = MapType::mapped_type;

    void setup(const TestConfig& config)
    {
        TestBase::setup(config);

        const size_t n = config.map_size;
        std::mt19937_64 rng(12345);
        std::uniform_int_distribution<Key> dist(0, static_cast<Key>(n * 10));

        for (size_t i = 0; i < n; ++i)
            keys.push_back(dist(rng));

        m_reps = m_config.op_count / m_config.map_size;
        m_reps = std::max(size_t(1), m_reps);
    }

    void run()
    {
        MapType m;
        for (size_t j = 0; j < m_reps; ++j)
        {
            for (const auto& key : keys)
                map_insert(m, key, key);

            m.clear();
        }
    }

private:
    std::vector<Key> keys;
    size_t m_reps = 1;
};

template <typename MapType>
class FindTest : public TestBase
{
public:
    using Key = MapType::key_type;
    using Value = MapType::mapped_type;

    void setup(const TestConfig& config)
    {
        TestBase::setup(config);

        for (size_t i = 0; i < config.map_size; ++i)
            map_insert(m_map, static_cast<Key>(i), static_cast<Key>(i));

        m_rng = std::mt19937_64(12345);
        m_dist = std::uniform_int_distribution<Key>(0, static_cast<Key>(config.map_size - 1));
    }

    void run()
    {
        size_t found = 0;

        for (size_t i = 0; i < m_config.op_count; ++i)
        {
            Key key = m_dist(m_rng);
            if (map_find(m_map, key))
                ++found;
        }

        // Usar volatile found para evitar optimizaciones agresivas, aunque no requerido aquí
        volatile auto dummy = found;
    }

private:
    MapType m_map;
    std::mt19937_64 m_rng;
    std::uniform_int_distribution<Key> m_dist;
};

template <typename MapType>
class EraseTest : public TestBase
{
public:
    using Key = MapType::key_type;
    using Value = MapType::mapped_type;

    void setup(const TestConfig& config)
    {
        TestBase::setup(config);

        for (size_t i = 0; i < config.map_size; ++i)
            map_insert(m_initialMap, static_cast<Key>(i), static_cast<Key>(i));

        m_rng = std::mt19937_64(12345);
        m_dist = std::uniform_int_distribution<Key>(0, static_cast<Key>(config.map_size - 1));
    }

    void pre_run()
    {
        // Handles the case in whcih the number operations exceeds map size. Creates enough copies of the
        // map to make sure it will never run out of stuff to delete.
        m_maps.clear();
        size_t map_count = m_config.op_count / m_config.map_size;
        map_count = std::max(size_t(1), map_count);

        for (; map_count > 0; --map_count)
            m_maps.push_back(m_initialMap);

        m_mapIndex = 0;
    }

    void run()
    {
        m_mapIndex = 0;
        size_t erased = 0;
        for (size_t i = 0; i < m_config.op_count; ++i)
        {
            auto& map = m_maps[m_mapIndex];
            Key key = m_dist(m_rng);
            if (map_erase(map, key))
            {
                ++erased;
                if (map.empty())
                    ++m_mapIndex;
            }
        }
    }

private:
    MapType m_initialMap;
    std::vector<MapType> m_maps;
    std::mt19937_64 m_rng;
    std::uniform_int_distribution<Key> m_dist;
    size_t m_mapIndex = 0;
};

template <typename MapType>
class SeqReadTest : public TestBase
{
public:
    using Key = MapType::key_type;
    using Value = MapType::mapped_type;

    void setup(const TestConfig& config)
    {
        TestBase::setup(config);

        for (size_t i = 0; i < config.map_size; ++i)
            map_insert(m_map, static_cast<Key>(i), static_cast<Value>(i));

        m_reps = m_config.op_count / m_config.map_size;
        m_reps = std::max(size_t(1), m_reps);

        // This test is a bit too fast, so lets increase the repetitions a little
        m_reps *= 20;
    }

    void run()
    {
        volatile Value sink;
        for (size_t j = 0; j < m_reps; ++j)
        {
            for (const auto& kv : m_map)
                sink = get_value(kv);
        }
    }

private:
    MapType m_map;
    size_t m_reps = 1;
};

size_t calc_repetitions(size_t size)
{
    const double maxReps = 299;
    const double minReps = 3;
    const double maxRepsLogSize = 1;
    const double minRepsLogSize = 5.2;
    const double k = (minReps - maxReps) / (minRepsLogSize - maxRepsLogSize);
    const double c = maxReps - maxRepsLogSize * k;

    const double logSize = std::log10(double(size));
    double dReps = logSize * k + c;

    dReps = std::max(dReps, minReps);
    dReps = std::min(dReps, maxReps);

    size_t iReps = size_t(std::round(dReps));

    // Should be odd.
    if ((iReps & 1) == 0)
        ++iReps;

    return iReps;
}

template <typename BenchmarkType>
BenchmarkResult run_benchmark(TestConfig config, const std::string& operation)
{
    // const size_t size = params.map_size;
    // const size_t nReps = calc_repetitions(size);
    const size_t nReps = 7;
    std::vector<double> measurements;
    measurements.reserve(nReps);
    BenchmarkType test;

    config.operation = operation;

    // Setup (just once)
    test.setup(config);

    // Warmup
    test.pre_run();
    test.run();

    for (size_t i = 0; i < nReps; ++i)
    {
        test.pre_run();

        auto start = std::chrono::high_resolution_clock::now();
        test.run();
        auto end = std::chrono::high_resolution_clock::now();
        measurements.push_back(std::chrono::duration<double, std::milli>(end - start).count());
    }

    BenchmarkResult result;
    result.config = config;

    // Get the median
    // std::sort(measurements.begin(), measurements.end());
    // params.duration_ms = measurements[nReps / 2];

    // Get the average
    double acc = 0;
    for (double v : measurements)
        acc += v;

    result.duration_ms = acc / measurements.size();

    return result;
}

// Función variádica para ejecutar todas las pruebas para todos los mapas pasados y acumular resultados
template <typename... Maps>
std::vector<BenchmarkResult> run_all_benchmarks(
    const TestConfig& base_config,
    const std::vector<std::string>& map_names,
    Maps&&... maps
)
{
    std::vector<BenchmarkResult> results;

    auto run_for_map = [&](auto&& map_type, const std::string& name)
    {
        using MapT = std::decay_t<decltype(map_type)>;
        TestConfig config(base_config);
        config.map_name = name;

        results.push_back(run_benchmark<InsertionTest<MapT>>(config, "insertion"));
        results.push_back(run_benchmark<RandomInsertionTest<MapT>>(config, "insertion_random"));
        results.push_back(run_benchmark<FindTest<MapT>>(config, "find"));
        results.push_back(run_benchmark<EraseTest<MapT>>(config, "erase"));
        results.push_back(run_benchmark<SeqReadTest<MapT>>(config, "sequential_read"));
    };

    size_t idx = 0;
    (run_for_map(std::forward<Maps>(maps), map_names[idx++]), ...);

    return results;
}

const BenchmarkResult* find_result(
    const std::vector<BenchmarkResult>& results,
    const std::string& map_name,
    const std::string& operation,
    size_t map_size
)
{
    auto it = std::find_if(
        results.begin(),
        results.end(),
        [&](const BenchmarkResult& r)
        {
            const auto& c = r.config;
            return c.map_name == map_name && c.operation == operation && c.map_size == map_size;
        }
    );

    if (it != results.end())
        return &(*it);
    else
        return nullptr;
}

// Imprime tablas con filas para cada mapa y columnas para cada tamaño, filtrando por operación
void print_results_table(
    const std::vector<BenchmarkResult>& results,
    const std::string& operation,
    const std::vector<std::string>& map_names,
    const std::vector<size_t>& map_sizes,
    std::ostream& output
)
{
    output << "\n# Operation: " << operation << "\n\n";

    output << std::setw(25) << "Configuration";
    for (auto size : map_sizes)
        output << std::setw(15) << size;
    output << '\n';

    for (const auto& map_name : map_names)
    {
        output << std::setw(25) << std::setprecision(4) << map_name;
        for (auto size : map_sizes)
        {
            const auto* result = find_result(results, map_name, operation, size);

            if (result != nullptr)
                output << std::setw(15) << result->duration_ms;
            else
                output << std::setw(15) << "-";
        }
        output << '\n';
    }
}

void print_results_csv_header(std::ostream& output, char separator = ';')
{
    // clang-format off
    output << "operation" 
        << separator << "config" 
        << separator << "size" 
        << separator << "time_ms" 
        << "\n";
    // clang-format on
}

void print_results_csv(
    const std::vector<BenchmarkResult>& results,
    const std::string& operation,
    const std::vector<std::string>& map_names,
    const std::vector<size_t>& map_sizes,
    std::ostream& output,
    char separator = ';'
)
{
    for (const auto& map_name : map_names)
    {
        for (auto size : map_sizes)
        {
            const auto* result = find_result(results, map_name, operation, size);

            if (result != nullptr)
            {
                // clang-format off
                output << operation 
                    << ";" << map_name 
                    << ";" << size 
                    << ";" << std::setprecision(7) << result->duration_ms 
                    << "\n";
                // clang-format on
            }
        }
    }
}

int main()
{
    // clang-format off
    std::vector<TestConfig> base_configs = 
    {
        {"", "", 10, 100'000},
        {"", "", 100, 100'000},
        {"", "", 1'000, 100'000},
        {"", "", 10'000, 100'000},
        {"", "", 100'000, 100'000},
        {"", "", 1'000'000, 100'000}
    };
    // clang-format on
    std::vector<size_t> map_sizes;

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
    TestConfig config;

    for (auto& config : base_configs)
        map_sizes.push_back(config.map_size);

    for (auto& config : base_configs)
    {
        // const size_t reps = calc_repetitions(n);
        // std::cerr << "Running tests for size: " << n << " (" << reps << " reps)...";
        std::cerr << "Running tests for config (" << config.map_size << ", " << config.op_count
                  << ")...";

        auto start = std::chrono::high_resolution_clock::now();

        auto results = run_all_benchmarks(
            config,
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

        auto end = std::chrono::high_resolution_clock::now();
        const double seconds = std::chrono::duration<double>(end - start).count();
        std::cerr << std::setprecision(3) << " (" << seconds << "s)\n";
    }

    std::array operations {"insertion", "insertion_random", "find", "erase", "sequential_read"};

    std::cout << "\n--- CSV ---\n\n";
    print_results_csv_header(std::cout);

    for (auto& op : operations)
        print_results_csv(all_results, op, map_names, map_sizes, std::cout);

    std::cout << "\n--- FORMATTED ---\n";

    for (auto& op : operations)
        print_results_table(all_results, op, map_names, map_sizes, std::cout);

    return 0;
}