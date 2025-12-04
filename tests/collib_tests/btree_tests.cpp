#include "pch-collib-tests.h"

#include "bmap.h"
#include "btree_checker.h"
#include "life_cycle_object.h"
#include "mem_check_fixture.h"

using namespace coll;

template <typename Map>
static bool checkMap(const Map& map)
{
    auto mapChecker = makeBtreeChecker(map);
    auto report = mapChecker.check();

    for (const auto& message : report)
        WARN("BTree health check: " + std::string(message));

    return report.empty();
}

class BTreeTests : public MemCheckFixture
{
};

TEST_CASE_METHOD(BTreeTests, "Pruebas básicas de bmap", "[std_map]")
{
    bmap<LifeCycleObject, std::string> m;

    SECTION("Inserción y acceso básico")
    {
        m.insert({1, "uno"});
        m[2] = "dos";
        REQUIRE(m.size() == 2);
        REQUIRE(m[1] == "uno");
        REQUIRE(m[2] == "dos");
    }

    SECTION("Búsqueda de claves")
    {
        m[10] = "diez";
        auto it = m.find(10);
        REQUIRE(it);
        CHECK(it.value() == "diez");
        CHECK(m.find(99) == false);
        CHECK(m.count(10) == 1);
        CHECK(m.count(99) == 0);
        CHECK(m.contains(10));
        CHECK_FALSE(m.contains(99));
    }

    SECTION("Sobrescritura de valores")
    {
        m[5] = "cinco";
        REQUIRE(m[5] == "cinco");
        m[5] = "cinco_modificado";
        REQUIRE(m[5] == "cinco_modificado");
    }

    SECTION("Eliminación de elementos")
    {
        m[3] = "tres";
        REQUIRE(m.size() == 1);
        m.erase(3);
        REQUIRE(m.size() == 0);
        REQUIRE(m.find(3) == false);
    }

    SECTION("Ordenación automática por clave")
    {
        m[20] = "veinte";
        m[5] = "cinco";
        m[15] = "quince";

        std::vector<int> keys;
        for (const auto& pair : m)
        {
            keys.push_back(pair.key.value());
        }

        REQUIRE(keys == std::vector<int> {5, 15, 20});
    }

    SECTION("Acceso con at() y manejo de excepción")
    {
        m[1] = "uno";
        REQUIRE(m.at(1) == "uno");
        REQUIRE_THROWS_AS(m.at(99), std::out_of_range);
    }

    m.clear();
}

TEST_CASE_METHOD(BTreeTests, "Constructores y operadores de bmap", "[std_map][copy_move]")
{
    SECTION("Constructor copia")
    {
        bmap<LifeCycleObject, std::string> original {{1, "uno"}, {2, "dos"}};
        bmap<LifeCycleObject, std::string> copia(original);
        REQUIRE(copia.size() == original.size());
        REQUIRE(copia == original);
    }

    SECTION("Constructor movimiento")
    {
        bmap<LifeCycleObject, std::string> original {{1, "uno"}, {2, "dos"}};
        bmap<LifeCycleObject, std::string> movido(std::move(original));
        REQUIRE(movido.size() == 2);
        REQUIRE(movido.find(1).value() == "uno");
        REQUIRE(original.empty()); // original queda vacío tras move
    }

    SECTION("Operador de asignación copia")
    {
        bmap<LifeCycleObject, std::string> source {{1, "uno"}};
        bmap<LifeCycleObject, std::string> target;
        target = source;
        REQUIRE(target == source);
    }

    SECTION("Operador de asignación movimiento")
    {
        bmap<LifeCycleObject, std::string> source {{1, "uno"}, {2, "dos"}};
        bmap<LifeCycleObject, std::string> target;
        target = std::move(source);
        REQUIRE(target.size() == 2);
        REQUIRE(source.empty()); // source queda vacío tras move
    }
}

TEST_CASE_METHOD(
    BTreeTests,
    "Recorrido con range for de bmap: valores, directo e inverso",
    "[std_map][iteration]"
)
{
    bmap<LifeCycleObject, std::string>
        m {{1, "uno"}, {3, "tres"}, {2, "dos"}, {5, "cinco"}, {4, "cuatro"}};

    SECTION("Recorrido directo (valores en orden de claves crecientes)")
    {
        std::vector<std::string> valores_obtenidos;
        for (const auto& [clave, valor] : m)
        {
            valores_obtenidos.push_back(valor);
        }
        REQUIRE(valores_obtenidos == std::vector<std::string> {"uno", "dos", "tres", "cuatro", "cinco"});
    }

    SECTION("Recorrido inverso (valores en orden de claves decrecientes)")
    {
        std::vector<std::string> valores_obtenidos;
        for (auto it = m.rbegin(); it != m.rend(); ++it)
        {
            valores_obtenidos.push_back(it.value());
        }
        REQUIRE(valores_obtenidos == std::vector<std::string> {"cinco", "cuatro", "tres", "dos", "uno"});
    }

    SECTION("Recorrido inverso (valores en orden de claves decrecientes) - Range for")
    {
        std::vector<std::string> valores_obtenidos;
        for (auto entry : m.rbegin())
        {
            valores_obtenidos.push_back(entry.value);
        }
        REQUIRE(valores_obtenidos == std::vector<std::string> {"cinco", "cuatro", "tres", "dos", "uno"});
    }
}

TEST_CASE_METHOD(BTreeTests, "bmap clear()", "[std_map][clear]")
{
    bmap<LifeCycleObject, std::string> m = {{1, "uno"}, {2, "dos"}, {3, "tres"}};

    REQUIRE(!m.empty());
    REQUIRE(m.size() == 3);

    m.clear();

    REQUIRE(m.empty());
    REQUIRE(m.size() == 0);

    // Verificar que no se encuentran elementos tras clear
    REQUIRE(m.find(1) == false);
    REQUIRE(m.find(2) == false);
    REQUIRE(m.find(3) == false);

    // Clear en mapa ya vacío no falla ni modifica tamaño
    m.clear();
    REQUIRE(m.empty());
    REQUIRE(m.size() == 0);
}

TEST_CASE_METHOD(
    BTreeTests,
    "bmap insert, emplace y try_emplace",
    "[std_map][insert][emplace][try_emplace]"
)
{
    bmap<LifeCycleObject, std::string> m;

    SECTION("insert con par clave-valor")
    {
        auto result = m.insert(1, "uno");
        REQUIRE(result.inserted == true);
        REQUIRE(result.location.key() == 1);
        REQUIRE(result.location.value() == "uno");

        // Intento de insertar misma clave no modifica el valor
        auto result2 = m.insert({1, "modificado"});
        REQUIRE(result2.inserted == false);
        REQUIRE(result2.location.value() == "uno"); // valor no modificado
    }

    SECTION("emplace construye elemento en sitio")
    {
        auto result = m.emplace(2, "dos");
        REQUIRE(result.inserted == true);
        REQUIRE(result.location.key() == 2);
        REQUIRE(result.location.value() == "dos");

        // Intento de emplace con clave existente no modifica el valor
        std::string modificado = "modificado";
        auto result2 = m.emplace(2, std::move(modificado));
        REQUIRE(result2.inserted == false);
        REQUIRE(result.location.value() == "dos");
        REQUIRE(modificado == "modificado"); // no se movió 'modificado'
    }
}

TEST_CASE_METHOD(BTreeTests, "bmap insert_or_assign()", "[std_map][insert_or_assign]")
{
    bmap<LifeCycleObject, std::string> m;

    // Inserción de un nuevo elemento
    auto resultado1 = m.insert_or_assign(1, "uno");
    REQUIRE(resultado1.inserted == true);
    REQUIRE(resultado1.location.key() == 1);
    REQUIRE(resultado1.location.value() == "uno");
    REQUIRE(m.size() == 1);

    // Asignación (actualización) de valor en clave existente
    auto resultado2 = m.insert_or_assign(1, "modificado");
    REQUIRE(resultado2.inserted == false);
    REQUIRE(resultado2.location.key() == 1);
    REQUIRE(resultado2.location.value() == "modificado");
    REQUIRE(m.size() == 1);

    // Inserción de otro elemento nuevo
    auto resultado3 = m.insert_or_assign(2, "dos");
    REQUIRE(resultado3.inserted == true);
    REQUIRE(resultado3.location.key() == 2);
    REQUIRE(resultado3.location.value() == "dos");
    REQUIRE(m.size() == 2);
}

#if 0
TEST_CASE_METHOD(BTreeTests, "bmap insert(range) desde iteradores", "[std_map][insert_range]") {
	bmap<LifeCycleObject, std::string> m{
		{1, "uno"},
		{4, "cuatro"}
	};

	std::vector<std::pair<int, std::string>> datos_a_insertar{
		{2, "dos"},
		{3, "tres"},
		{4, "cuatro_modificado"}, // clave duplicada, no se insertará
		{5, "cinco"}
	};

	m.insert(datos_a_insertar.begin(), datos_a_insertar.end());

	CHECK(m.size() == 5);
	CHECK(m[1] == "uno");         // existente sin cambio
	CHECK(m[2] == "dos");         // nuevo insertado
	CHECK(m[3] == "tres");        // nuevo insertado
	CHECK(m[4] == "cuatro");      // existente sin cambio, no modificado
	CHECK(m[5] == "cinco");       // nuevo insertado
}

TEST_CASE_METHOD(BTreeTests, "bmap erase()", "[std_map][erase]") {
	bmap<LifeCycleObject, std::string> m{
		{1, "uno"},
		{2, "dos"},
		{3, "tres"},
		{4, "cuatro"},
		{5, "cinco"}
	};

	SECTION("Erase por clave existente") {
		size_t cantidad = m.erase(3);
		CHECK(cantidad == 1);
		CHECK(m.size() == 4);
		CHECK(m.find(3) == m.end());
	}

	SECTION("Erase por clave no existente") {
		size_t cantidad = m.erase(99);
		CHECK(cantidad == 0);
		CHECK(m.size() == 5);
	}

	SECTION("Erase por iterador") {
		auto it = m.find(2);
		CHECK(it != m.end());
		auto siguiente = m.erase(it);
		CHECK(siguiente->first == 3);
		CHECK(m.size() == 4);
		CHECK(m.find(2) == m.end());
	}

	SECTION("Erase por rango de iteradores") {
		auto it_inicio = m.find(2);
		auto it_fin = m.find(5);
		auto it_retorno = m.erase(it_inicio, it_fin); // elimina claves 2,3,4
		CHECK(it_retorno->first == 5);
		CHECK(m.size() == 2);
		CHECK(m.find(2) == m.end());
		CHECK(m.find(3) == m.end());
		CHECK(m.find(4) == m.end());
	}
}

TEST_CASE_METHOD(BTreeTests, "bmap merge()", "[std_map][merge]") {
	bmap<LifeCycleObject, std::string> destino{
		{1, "uno"},
		{2, "dos"}
	};
	bmap<LifeCycleObject, std::string> fuente{
		{2, "dos_fuente"},   // clave duplicada, no será movida
		{3, "tres"},
		{4, "cuatro"}
	};

	destino.merge(fuente);

	CHECK(destino.size() == 4);
	CHECK(destino[1] == "uno");
	CHECK(destino[2] == "dos");
	CHECK(destino[3] == "tres");
	CHECK(destino[4] == "cuatro");

	CHECK(fuente.size() == 1);
	CHECK(fuente.find(2) != fuente.end()); // clave duplicada queda en fuente
	CHECK(fuente[2] == "dos_fuente");
}
#endif
TEST_CASE_METHOD(BTreeTests, "bmap find() y contains()", "[std_map][find][contains]")
{
    bmap<LifeCycleObject, std::string> m {{1, "uno"}, {2, "dos"}, {3, "tres"}};

    SECTION("find clave existente")
    {
        auto it = m.find(2);
        CHECK(it);
        CHECK(it.key() == 2);
        CHECK(it.value() == "dos");
    }

    SECTION("find clave no existente")
    {
        auto it = m.find(99);
        CHECK(it == false);
    }

    SECTION("contains clave existente") { CHECK(m.contains(1)); }

    SECTION("contains clave no existente") { CHECK_FALSE(m.contains(42)); }
}

class SimpleKey
{
public:
    SimpleKey(const char* text)
        : m_text(text)
    {
    }
    SimpleKey(std::string_view text)
        : m_text(text)
    {
    }
    SimpleKey(const SimpleKey&) = default;
    SimpleKey(SimpleKey&&) = default;

    SimpleKey& operator=(const SimpleKey&) = default;
    SimpleKey& operator=(SimpleKey&&) = default;

    bool operator<(const SimpleKey& rhs) const { return m_text < rhs.m_text; }
    bool operator==(const SimpleKey& rhs) const { return m_text == rhs.m_text; }

private:
    std::string m_text;
};

TEST_CASE_METHOD(
    BTreeTests,
    "Prueba el árbol con una clave que sólo cumple requisitos mínimos",
    "[std_map][simple_key]"
)
{
    bmap<SimpleKey, int> m {{"uno", 1}, {"dos", 2}, {"tres", 3}};

    SECTION("find clave existente")
    {
        auto it = m.find("dos");
        CHECK(it);
        CHECK(it.key() == "dos");
        CHECK(it.value() == 2);
    }

    SECTION("find clave no existente")
    {
        auto it = m.find("no_existe");
        CHECK(it == false);
    }

    SECTION("contains clave existente") { CHECK(m.contains("uno")); }

    SECTION("contains clave no existente") { CHECK_FALSE(m.contains("cuarenta y dos")); }
}

TEST_CASE_METHOD(
    BTreeTests,
    "bmap inserciones masivas: provoca división de nodos hojas e internos",
    "[btree][split_nodes]"
)
{
    bmap<LifeCycleObject, std::string> m;

    const int total_insertions = 50;

    for (int i = 1; i <= total_insertions; ++i)
        m.insert(i, std::to_string(i));

    REQUIRE(m.size() == total_insertions);

    // Verificar que todas las claves están presentes y en orden
    LifeCycleObject prev_key = 0;
    for (const auto& [k, v] : m)
    {
        CHECK(k == std::stoi(v));
        CHECK(k > prev_key);
        prev_key = k;
    }

    // Verificar que el primer y último valor son correctos
    CHECK(m.begin().key() == 1);
    CHECK(m.rbegin().key() == total_insertions);
}

TEST_CASE_METHOD(BTreeTests, "bmap: random inserts", "[btree][split_nodes][random]")
{
    bmap<LifeCycleObject, std::string, 4> m;
    const int count = 1024;

    std::mt19937_64 rng(12345);
    std::uniform_int_distribution<int> dist(0, count * 5);

    for (size_t i = 0; i < count; ++i)
    {
        const int value = dist(rng);
        m[value] = std::to_string(value);
    }

    CHECK(checkMap(m));
}

TEST_CASE_METHOD(BTreeTests, "bmap: Interleaved inserts to expose bug", "[btree][bug][linked_list]")
{
    // This test is designed to expose a bug found inserting nodes, that leaf nodes linked list was not
    // updated properly.
    bmap<LifeCycleObject, int, 4> m;

    auto insertSeq = [&m](int start, int count)
    {
        for (int i = start; i < start + count; ++i)
            m[i] = i * 5 + 23;
    };

    insertSeq(0, 8);
    insertSeq(32, 8);
    CHECK(checkMap(m));

    insertSeq(16, 8);
    CHECK(checkMap(m));
}

TEST_CASE_METHOD(BTreeTests, "bmap borrados masivos con reequilibrado", "[btree][erase][rebalance]")
{
    bmap<LifeCycleObject, std::string, 4> m;
    const int total_insertions = 40;

    for (int i = 0; i < total_insertions; ++i)
        m.insert(i, "v" + std::to_string(i));

    REQUIRE(m.size() == total_insertions);

    int expectedKey = 0;
    for (const auto& entry : m)
    {
        std::string expectedValue = "v" + std::to_string(expectedKey);
        CHECK(entry.key == expectedKey);
        CHECK(entry.value == expectedValue);
        ++expectedKey;
    }

    CHECK(checkMap(m));

    // This erase pattern will trigger rebalances, but no merges.
    for (int i = 0; i < total_insertions; i += 2)
    {
        INFO("Deleting node: " << i);
        CHECK(m.erase(i));
    }

    REQUIRE(m.size() == total_insertions / 2);
    CHECK(checkMap(m));

    // This second round of erases will trigger some merges.
    for (int i = 3; i < total_insertions; i += 3)
        m.erase(i);

    CHECK(checkMap(m));

    int previo = -1;
    for (const auto& [k, v] : m)
    {
        CHECK(k.value() % 2 == 1);
        CHECK(k > previo);
        previo = k.value();
    }

    // Verificar búsqueda negativa y positiva
    CHECK(m.find(1) == true);
    CHECK(m.find(2) == false);
    CHECK_FALSE(m.contains(0));
    CHECK(m.contains(5));
}

TEST_CASE_METHOD(
    BTreeTests,
    "bmap borrados masivos con reequilibrado: Segmentos grandes del árbol",
    "[btree][erase][rebalance]"
)
{
    bmap<LifeCycleObject, LifeCycleObject> m;
    const int total = 4096;
    const int segment = 0x200;

    // Inserciones
    for (int i = 0; i < total; ++i)
        m.insert(i, i * 23);

    CHECK(checkMap(m));
    REQUIRE(m.size() == total);

    // Eliminaciones parciales
    size_t deleteCount = 0;
    for (int i = 0; i < total; ++i)
    {
        if (i & segment)
        {
            m.erase(i);
            ++deleteCount;
            // INFO("Delete count is: " << deleteCount);
            // REQUIRE(checkMap(m));
        }
    }
    REQUIRE(m.size() == total - deleteCount);
    CHECK(checkMap(m));

    for (auto entry : m)
        CHECK((entry.key.value() & segment) == 0);

    // Finally, delete everything, one by one.
    for (int i = 0; i < total; ++i)
        m.erase(i);

    CHECK(m.empty());
    CHECK(checkMap(m));
}

TEST_CASE_METHOD(
    BTreeTests,
    "bmap prueba estrés: inserciones, borrados y reinserciones",
    "[btree][stress]"
)
{
    bmap<LifeCycleObject, LifeCycleObject> m;
    const int total = 2000;

    // Inserciones
    for (int i = 0; i < total; ++i)
        m.insert(i, i * i);

    CHECK(checkMap(m));
    REQUIRE(m.size() == total);

    // Eliminaciones parciales
    size_t deleteCount = 0;
    for (int i = 0; i < total; i += 3)
    {
        m.erase(i);
        ++deleteCount;
    }
    REQUIRE(m.size() == total - deleteCount);
    CHECK(checkMap(m));

    // Reinserciones de algunas claves borradas
    for (int i = 0; i < total; i += 6)
        m.insert(i, i * i);

    CHECK(checkMap(m));

    // Validar integridad total: todas las claves esperadas deben existir
    for (int i = 0; i < total; ++i)
    {
        auto it = m.find(i);
        if (i % 3 != 0 || i % 6 == 0)
            CHECK(it);
        else
            CHECK(!it);
    }
}
