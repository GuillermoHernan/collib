#include "pch-collib-tests.h"
#include "darray.h"
#include <list>
#include <string>

using namespace coll;

TEST_CASE("Pruebas básicas de darray", "[darray]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("Constructor sin elementos crea array vacío") {
        darray<int> da(alloc);
        CHECK(da.empty());
        CHECK(da.size() == 0);
        CHECK(da.capacity() == 0);
    }

    SECTION("Constructor con tamaño reserva memoria y crea elementos por defecto") {
        darray<int> da(5, alloc);
        CHECK(!da.empty());
        CHECK(da.size() == 5);
        for (size_t i = 0; i < da.size(); ++i) {
            CHECK(da[i] == 0); // int default initialized = 0
        }
    }

    SECTION("Constructor con tamaño y valor inicial") {
        darray<int> da(3, 7, alloc);
        CHECK(da.size() == 3);
        for (size_t i = 0; i < da.size(); ++i) {
            CHECK(da[i] == 7);
        }
    }

    SECTION("Constructor por rango desde std::vector") {
        std::vector<int> v = { 1, 2, 3 };
        darray<int> da(v);
        CHECK(da.size() == v.size());
        for (size_t i = 0; i < da.size(); ++i) {
            CHECK(da[i] == v[i]);
        }
    }

    SECTION("Constructor por std::initializer_list") {
        darray<std::string> da({ "foo", "bar", "baz" });
        CHECK(da.size() == 3);
        CHECK(da[0] == "foo");
        CHECK(da[1] == "bar");
        CHECK(da[2] == "baz");
    }

    SECTION("Asignación copia y movimiento") {
        darray<int> da1(3, 5, alloc);
        darray<int> da2 = da1;
        CHECK(da2.size() == da1.size());
        for (size_t i = 0; i < da2.size(); ++i) {
            CHECK(da2[i] == 5);
        }

        darray<int> da3 = std::move(da2);
        CHECK(da3.size() == 3);
        CHECK(da2.size() == 0);
    }

    SECTION("Asignación lista de inicialización") {
        darray<int> da(alloc);
        da = { 10, 20, 30 };
        CHECK(da.size() == 3);
        CHECK(da[0] == 10);
        CHECK(da[1] == 20);
        CHECK(da[2] == 30);
    }

    SECTION("Función clear") {
        darray<int> da(3, alloc);
        da.clear();
        CHECK(da.size() == 0);
        CHECK(da.empty());
    }

    SECTION("reserve aumenta capacidad sin cambiar tamaño") {
        darray<int> da(alloc);
        da.reserve(10);
        CHECK(da.capacity() >= 10);
        CHECK(da.size() == 0);
    }

    SECTION("Acceso por front, back y at") {
        darray<int> da = { 1, 2, 3 };
        CHECK(da.front() == 1);
        CHECK(da.back() == 3);
        CHECK(da.at(1) == 2);
    }
}

TEST_CASE("darray: Constructor con rango fuente sin size()", "[darray][constructor][range]") {
    IAllocator& alloc = defaultAllocator();

    std::forward_list<int> sourceList = { 10, 20, 30, 40 };
    darray<int> da(sourceList);

    size_t expected_size = 4; // forward_list no tiene size(), pero conocemos tamaño
    CHECK(da.size() == expected_size);

    size_t idx = 0;
    for (int val : sourceList) {
        CHECK(da[idx++] == val);
    }
}

TEST_CASE("darray: Constructor con rango fuente sin size()", "[darray]") {
    std::list<int> sourceList = { 10, 20, 30 }; // std::list no tiene size() accesible en misma forma que vector
    darray<int> da(sourceList);
    CHECK(da.size() == sourceList.size());
    size_t i = 0;
    for (auto val : sourceList) {
        CHECK(da[i++] == val);
    }
}

TEST_CASE("darray: Operador= copia mismo objeto no hace nada", "[darray]") {
    IAllocator& alloc = defaultAllocator();
    darray<int> da(3, alloc);
    da[0] = 1; da[1] = 2; da[2] = 3;

    da = da; // autoasignación
    CHECK(da.size() == 3);
    CHECK(da[0] == 1);
    CHECK(da[1] == 2);
    CHECK(da[2] == 3);
}

TEST_CASE("darray: Operador= copia objeto diferente", "[darray]") {
    IAllocator& alloc = defaultAllocator();
    darray<int> da1(3, alloc);
    da1[0] = 1; da1[1] = 2; da1[2] = 3;

    darray<int> da2(alloc);
    da2 = da1;
    CHECK(da2.size() == 3);
    CHECK(da2[0] == 1);
    CHECK(da2[1] == 2);
    CHECK(da2[2] == 3);
}

TEST_CASE("Operador de asignación copia darray (sobrescribir objeto existente)", "[darray]") {
    IAllocator& alloc = defaultAllocator();

    darray<int> da1(3, alloc);
    da1[0] = 42;
    da1[1] = 43;
    da1[2] = 44;

    darray<int> da2(5, alloc);
    da2[0] = 100;
    da2[1] = 200;
    da2[2] = 300;
    da2[3] = 400;
    da2[4] = 500;

    da2 = da1;

    CHECK(da2.size() == da1.size());
    CHECK(da2[0] == 42);
    CHECK(da2[1] == 43);
    CHECK(da2[2] == 44);
}

TEST_CASE("Operador de movimiento darray", "[darray]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("Mover darray transfiere recursos correctamente (construcción)") {
        darray<int> da1(3, alloc);
        da1[0] = 10;
        da1[1] = 20;
        da1[2] = 30;

        darray<int> da2 = std::move(da1);

        CHECK(da2.size() == 3);
        CHECK(da2[0] == 10);
        CHECK(da2[1] == 20);
        CHECK(da2[2] == 30);

        CHECK(da1.size() == 0);
        CHECK(da1.capacity() == 0);
        CHECK(da1.data() == nullptr);
    }

    SECTION("Operador de asignación por movimiento transfiere correctamente (objeto existente)") {
        darray<int> da1(2, alloc);
        da1[0] = 15;
        da1[1] = 25;

        darray<int> da2(3, alloc);
        da2[0] = 111;
        da2[1] = 222;
        da2[2] = 333;

        da2 = std::move(da1);

        CHECK(da2.size() == 2);
        CHECK(da2[0] == 15);
        CHECK(da2[1] == 25);

        CHECK(da1.size() == 0);
        CHECK(da1.capacity() == 0);
        CHECK(da1.data() == nullptr);
    }
}

TEST_CASE("darray: Accesos fuera de rango lanzan excepciones", "[darray]") {
    IAllocator& alloc = defaultAllocator();
    darray<int> da(3, alloc);
    for (size_t i = 0; i < 3; ++i)
        da[i] = (int)i;

    CHECK_THROWS_AS(da.at(3), std::out_of_range);
    CHECK_THROWS_AS(da.at(100), std::out_of_range);

    darray<int> empty(alloc);
    CHECK_THROWS_AS(empty.front(), std::out_of_range);
    CHECK_THROWS_AS(empty.back(), std::out_of_range);
}

TEST_CASE("darray: Métodos para añadir datos (push_back y emplace_back)", "[darray][push_back][emplace_back]") {
    IAllocator& alloc = defaultAllocator();
    darray<std::string> da(alloc);

    SECTION("push_back con copia") {
        std::string val = "push_copy";
        da.push_back(val);
        CHECK(da.size() == 1);
        CHECK(da[0] == val);
    }

    SECTION("push_back con movimiento") {
        std::string val = "push_move";
        da.push_back(std::move(val));
        CHECK(da.size() == 1);
        CHECK(da[0] == "push_move");
        CHECK(val.empty());
    }

    SECTION("emplace_back con construcción in-place") {
        da.emplace_back(5, 'c');
        CHECK(da.size() == 1);
        CHECK(da.front() == "ccccc");
    }

    SECTION("Inserción masiva para estrés por crecimiento dinámico") {
        size_t const large_num = 10000;
        darray<int> da_large(alloc);
        for (size_t i = 0; i < large_num; ++i) {
            da_large.push_back(int(i));
        }
        CHECK(da_large.size() == large_num);
        CHECK(da_large.capacity() >= large_num);
        bool correct = true;
        for (size_t i = 0; i < large_num; ++i) {
            if (da_large[i] != int(i)) {
                correct = false;
                break;
            }
        }
        CHECK(correct);
    }
}

TEST_CASE("darray: insert variantes", "[darray][insert]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("insert(const_iterator, const Item&)") {
        darray<int> da = { 1, 2, 3, 5, 6 };
        const int element = 4;
        auto it = da.insert(3, element);
        CHECK(da.size() == 6);
        CHECK(da[3] == 4);
        CHECK(da[4] == 5);
        CHECK(*it == 4);
    }

    SECTION("insert(const_iterator, Item&&)") {
        darray<int> da = { 1, 3, 4, 5 };
        int val = 2;
        auto it = da.insert(1, std::move(val)); // Insertar 2 en posición 1
        CHECK(da.size() == 5);
        CHECK(da[1] == 2);
        CHECK(*it == 2);
    }

    SECTION("insert(const_iterator, size_t count, const Item&) con count > 0") {
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(1, 2, 2); // Insertar dos 2s en posición 1
        CHECK(da.size() == 5);
        CHECK(da[1] == 2);
        CHECK(da[2] == 2);
        CHECK(da[3] == 4);
        CHECK(*it == 2);
    }

    SECTION("insert(const_iterator, size_t count, const Item&) con count == 0") {
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(1, 0, 99); // Insertar cero veces, sin cambios
        CHECK(da.size() == 3);
        CHECK(da[1] == 4);
        CHECK(*it == 4);
    }

    SECTION("insert(const_iterator, std::initializer_list<Item>) con lista no vacía") {
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(1, { 2, 2 }); // Insertar dos 2s en posición 1
        CHECK(da.size() == 5);
        CHECK(da[1] == 2);
        CHECK(da[2] == 2);
        CHECK(*it == 2);
    }

    SECTION("insert(const_iterator, std::initializer_list<Item>) con lista vacía") {
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(2, {}); // Insertar lista vacía, sin cambios
        CHECK(da.size() == 3);
        CHECK(*it == 5);
    }

    SECTION("insert(const_iterator, Range) con Range con size() no vacio") {
        std::vector<int> vec = { 2, 2 };
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(1, vec);
        CHECK(da.size() == 5);
        CHECK(da[1] == 2);
        CHECK(da[2] == 2);
        CHECK(*it == 2);
    }

    SECTION("insert(const_iterator, Range) con Range con size() vacío") {
        std::vector<int> vec;
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(1, vec);
        CHECK(da.size() == 3);
        CHECK(*it == 4);
    }

    SECTION("insert(const_iterator, Range) con Range sin size() no vacio") {
        std::forward_list<int> flist = { 2, 2 };
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(1, flist);
        CHECK(da.size() == 5);
        CHECK(da[0] == 1);
        CHECK(da[1] == 2);
        CHECK(da[2] == 2);
        CHECK(da[3] == 4);
        CHECK(*it == 2);
    }

    SECTION("insert(const_iterator, Range) con Range sin size() vacío") {
        std::forward_list<int> flist; // lista vacía
        darray<int> da = { 1, 4, 5 };
        auto it = da.insert(0, flist);
        CHECK(da.size() == 3);
        CHECK(*it == 1);
    }
}

TEST_CASE("darray: emplace en posición arbitraria", "[darray][emplace]") {
    IAllocator& alloc = defaultAllocator();

    darray<std::string> da = { "uno", "tres", "cuatro" };

    // Emplace "dos" en posición 1
    auto it = da.emplace(1, 3, 'd'); // construye "ddd"

    CHECK(*it == "ddd"); // el nuevo elemento insertado debe ser "ddd"
    CHECK(da.size() == 4);
    CHECK(da[0] == "uno");
    CHECK(da[1] == "ddd");
    CHECK(da[2] == "tres");
    CHECK(da[3] == "cuatro");
}

TEST_CASE("darray: append_range con rangos con y sin size", "[darray][append_range]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("append_range con std::vector (con size())") {
        darray<int> da = { 1, 2, 3 };
        std::vector<int> vec = { 4, 5, 6 };
        da.append_range(vec);

        CHECK(da.size() == 6);
        CHECK(da[3] == 4);
        CHECK(da[4] == 5);
        CHECK(da[5] == 6);
    }

    SECTION("append_range con std::forward_list (sin size())") {
        darray<int> da = { 1, 2, 3 };
        std::forward_list<int> flist = { 4, 5, 6 };
        da.append_range(flist);

        CHECK(da.size() == 6);
        CHECK(da[3] == 4);
        CHECK(da[4] == 5);
        CHECK(da[5] == 6);
    }

    SECTION("append_range con rango vacío") {
        darray<int> da = { 1, 2, 3 };
        std::vector<int> empty_vec;
        da.append_range(empty_vec);

        CHECK(da.size() == 3); // Sin cambios
    }
}

TEST_CASE("darray: pop_back elimina el último elemento", "[darray][pop_back]") {
    IAllocator& alloc = defaultAllocator();

    darray<int> da = { 1, 2, 3, 4 };
    CHECK(da.size() == 4);

    da.pop_back();
    CHECK(da.size() == 3);
    CHECK(da.back() == 3);

    da.pop_back();
    CHECK(da.size() == 2);
    CHECK(da.back() == 2);

    da.pop_back();
    da.pop_back();
    CHECK(da.size() == 0);
    CHECK(da.empty());

    // Comprobar que llamar pop_back en darray vacío lanza excepción
    CHECK_THROWS_AS(da.pop_back(), std::out_of_range);
}

TEST_CASE("darray: swap intercambia dos darrays", "[darray][swap]") {
    IAllocator& alloc = defaultAllocator();

    darray<int> da1 = { 1, 2, 3 };
    darray<int> da2 = { 4, 5 };

    da1.swap(da2);

    CHECK(da1.size() == 2);
    CHECK(da1[0] == 4);
    CHECK(da1[1] == 5);

    CHECK(da2.size() == 3);
    CHECK(da2[0] == 1);
    CHECK(da2[1] == 2);
    CHECK(da2[2] == 3);
}

TEST_CASE("darray: resize cambia tamaño y valores correctamente", "[darray][resize]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("resize aumenta tamaño con valor por defecto") {
        darray<int> da = { 1, 2, 3 };
        da.resize(5);
        CHECK(da.size() == 5);
        CHECK(da[0] == 1);
        CHECK(da[1] == 2);
        CHECK(da[2] == 3);
        CHECK(da[3] == 0); // valor por defecto int
        CHECK(da[4] == 0);
    }

    SECTION("resize disminuye tamaño") {
        darray<int> da = { 1, 2, 3, 4 };
        da.resize(2);
        CHECK(da.size() == 2);
        CHECK(da[0] == 1);
        CHECK(da[1] == 2);
    }

    SECTION("resize con valor explícito al crecer") {
        darray<int> da = { 1, 2 };
        da.resize(4, 7);
        CHECK(da.size() == 4);
        CHECK(da[0] == 1);
        CHECK(da[1] == 2);
        CHECK(da[2] == 7);
        CHECK(da[3] == 7);
    }

    SECTION("resize sin cambios") {
        darray<int> da = { 1, 2, 3 };
        da.resize(3);
        CHECK(da.size() == 3);
        CHECK(da[0] == 1);
        CHECK(da[1] == 2);
        CHECK(da[2] == 3);
    }
}

TEST_CASE("darray: assign - tamaño y valor", "[darray][assign]") {
    darray<int> da;
    da.assign(5, 42);
    REQUIRE(da.size() == 5);
    for (size_t i = 0; i < da.size(); ++i)
        REQUIRE(da[i] == 42);
}

TEST_CASE("darray: assign - initializer_list", "[darray][assign]") {
    darray<int> da;
    da.assign({ 1, 2, 3, 4 });
    REQUIRE(da.size() == 4);
    REQUIRE(da[0] == 1);
    REQUIRE(da[1] == 2);
    REQUIRE(da[2] == 3);
    REQUIRE(da[3] == 4);
}

TEST_CASE("darray: assign - rango", "[darray][assign]") {
    std::vector<int> v = { 10, 20, 30 };
    darray<int> da;
    da.assign(v);
    REQUIRE(da.size() == 3);
    REQUIRE(da[0] == 10);
    REQUIRE(da[1] == 20);
    REQUIRE(da[2] == 30);
}

TEST_CASE("darray: assign - asignación con count == 0 vacía", "[darray][assign]") {
    darray<int> da = { 1, 2, 3, 4 };
    da.assign(0, 42);
    REQUIRE(da.size() == 0);
    REQUIRE(da.empty());
}

TEST_CASE("darray: assign - asignación con initializer_list vacío", "[darray][assign]") {
    darray<int> da = { 1, 2, 3, 4 };
    da.assign({});
    REQUIRE(da.size() == 0);
    REQUIRE(da.empty());
}

TEST_CASE("darray: operator<=> comparación con otro darray y rango", "[darray][compare]") {

    darray<int> da1 = { 1, 2, 3 };
    darray<int> da2 = { 1, 2, 3 };
    darray<int> da3 = { 1, 2, 4 };
    darray<int> da4 = { 1, 2 };
    std::vector<int> vec = { 1, 2, 3 };
    std::vector<int> vec2 = { 1, 2, 4 };

    SECTION("Equal") {
        CHECK((da1 == da2));
    }

    SECTION("Ordering comparisions, same size") {
        CHECK(da1 < da3);
        CHECK(da3 > da1);
    }

    SECTION("Size based inequality") {
        CHECK(da4 < da1);
        CHECK(da1 > da4);
    }

    SECTION("Comparision with vector, equal") {
        CHECK(da1 == vec);
        CHECK(vec == da1);
    }

    SECTION("Comparision with vector, different") {
        CHECK(da1 < vec2);
        CHECK(vec2 > da1);
        CHECK(vec2 != da1);
        CHECK(da1 != vec2);
    }

    SECTION("Comparision with itself") {
        CHECK(da3 == da3);
    }
}
