#include "pch-collib-tests.h"

#include "span.h"
#include "vrange.h"

#include <list>
#include <vector>

using namespace coll;

TEST_CASE("vrange básico - construcción y acceso", "[vrange]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("Crear vrange desde std::vector y comprobar front y empty") {
        std::vector<int> v = { 1, 2, 3 };
        vrange<int> r(v, alloc);
        REQUIRE(!r.empty());
        CHECK(r.front() == 1);
        CHECK(*r == 1);
    }

    SECTION("Crear vrange desde std::list y comprobar iteración con operator++") {
        std::list<int> l = { 10, 20, 30 };
        vrange<int> r(l, alloc);
        CHECK(r.front() == 10);
        ++r;
        CHECK(r.front() == 20);
        r++;
        CHECK(r.front() == 30);
        ++r;
        CHECK(r.empty());
    }

    SECTION("Crear vrange desde std::forward_list y recorrer con while") {
        std::forward_list<int> fl = { 5, 6, 7 };
        vrange<int> r(fl, alloc);
        int expected[] = { 5, 6, 7 };
        size_t idx = 0;
        while (!r.empty()) {
            CHECK(r.front() == expected[idx++]);
            ++r;
        }
        CHECK(idx == 3);
    }
}

TEST_CASE("vrange copia y movimiento", "[vrange]") {
    IAllocator& alloc = defaultAllocator();
    std::vector<int> v = { 1, 2, 3 };

    SECTION("Copia") {
        vrange<int> r1(v, alloc);
        vrange<int> r2 = r1;
        CHECK(r2.front() == r1.front());
        ++r2;
        CHECK(r2.front() == 2);
        CHECK(!r1.empty()); // r1 original no cambiado
    }

    SECTION("Movimiento") {
        auto r1 = make_range(v, alloc);
        vrange<int> r2 = std::move(r1);
        CHECK(r2.front() == 1);
        CHECK(r1.empty()); // r1 queda vacío tras move
    }

    SECTION("Operador= copia y move asignación") {
        vrange<int> r1(v, alloc);
        vrange<int> r2(std::list<int>{4, 5}, alloc);
        r2 = r1;
        CHECK(r2.front() == 1);
        vrange<int> r3(std::vector<int>{6}, alloc);
        r3 = std::move(r2);
        CHECK(r3.front() == 1);
        CHECK(r2.empty());
    }
}

TEST_CASE("vrange acceso rango vacío y excepciones", "[vrange]") {
    IAllocator& alloc = defaultAllocator();

    SECTION("vrange vacío front() lanza excepción") {
        vrange<int> r;
        CHECK(r.empty());
        CHECK_THROWS_AS(r.front(), std::out_of_range);
    }

    SECTION("Comparar con Sentinel y vaciado tras iterar") {
        std::vector<int> v = { 1 };
        vrange<int> r(v, alloc);
        CHECK_FALSE(r == r.end());
        ++r;
        CHECK(r == r.end());
    }
}

TEST_CASE("vrange operadores y semánticas adicionales", "[vrange]") {
    IAllocator& alloc = defaultAllocator();
    std::vector<int> v = { 1, 2, 3 };
    vrange<int> r(v, alloc);

    SECTION("operator* es igual que front") {
        CHECK(*r == r.front());
    }

    SECTION("operator++ retorna referencia y copia") {
        vrange<int> r2 = r;
        vrange<int>& ref = (++r2);
        CHECK(ref.front() == 2);

        vrange<int> r3 = r;
        vrange<int> old = r3++;
        CHECK(old.front() == 1);
        CHECK(r3.front() == 2);
    }
}

TEST_CASE("vrange integrado con coll::span (forward)", "[vrange][span]") {
    IAllocator& alloc = defaultAllocator();

    int arr[] = { 10, 20, 30, 40 };

    span<int> sp(arr, 4);
    vrange<int> vr(sp, alloc);

    REQUIRE(!vr.empty());
    CHECK(vr.front() == 10);
    ++vr;
    CHECK(vr.front() == 20);
    vr++;
    CHECK(vr.front() == 30);
    ++vr;
    CHECK(vr.front() == 40);
    ++vr;
    CHECK(vr.empty());
}

TEST_CASE("vrange integrado con coll::span (reversed)", "[vrange][span][reverse]") {
    IAllocator& alloc = defaultAllocator();

    int arr[] = { 1, 2, 3, 4, 5 };
    span<int, true> rsp(arr, 5);
    vrange<int> vr(rsp, alloc);

    REQUIRE(!vr.empty());
    CHECK(vr.front() == 5);
    ++vr;
    CHECK(vr.front() == 4);
    vr++;
    CHECK(vr.front() == 3);
    ++vr;
    CHECK(vr.front() == 2);
    ++vr;
    CHECK(vr.front() == 1);
    ++vr;
    CHECK(vr.empty());
}

TEST_CASE("vrange con coll::span: operador* y comparisons", "[vrange][span]") {
    IAllocator& alloc = defaultAllocator();
    int arr[] = { 42, 43, 44 };
    span<int> sp(arr, 3);
    vrange<int> vr(sp, alloc);

    CHECK(*vr == vr.front());
    CHECK_FALSE(vr == vr.end());
    ++vr;
    ++vr;
    ++vr;
    CHECK(vr == vr.end());
}

//TEST_CASE("vrange con coll::span: excepciones en empty span", "[vrange][span]") {
//    IAllocator& alloc = defaultAllocator();
//    span<int> sp(nullptr, 0);
//    vrange<int> vr(sp, alloc);
//
//    CHECK(vr.empty());
//    CHECK_THROWS_AS(vr.front(), std::out_of_range);
//}
//
