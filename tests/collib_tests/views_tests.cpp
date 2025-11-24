#include "pch-collib-tests.h"

#include "darray.h"
#include "mem_check_fixture.h"
#include "vrange.h"

using namespace coll;

class ViewTests : public MemCheckFixture
{
};


TEST_CASE_METHOD(ViewTests,  "filter_view")
{
	SECTION("Filter view", "[filter_view]")
	{
		darray<int> data = { 3, 4, 0, 32, 29, 15, 72, 9, 1, 6 };
		darray<int> expected = { 3, 0, 15, 72, 9, 6 };

		vrange<int> filtered = make_range(data).filter([](int x) {return x % 3 == 0; });

		//std::cout << filtered << "\n";

		CHECK(expected == filtered);
	}

	SECTION("First items filtered", "[filter_view][first_item]")
	{
		darray<int> data = { 4, 5, 3, 0, 32, 29, 15, 72, 9, 1, 6 };
		darray<int> expected = { 3, 0, 15, 72, 9, 6 };

		vrange<int> filtered = make_range(data).filter([](int x) {return x % 3 == 0; });

		//std::cout << filtered << "\n";

		CHECK(expected == filtered);
	}

}

TEST_CASE_METHOD(ViewTests,  "filter")
{
	SECTION("Filter function", "[filter]")
	{
		darray<int> data = { 3, 4, 0, 32, 29, 15, 72, 9, 1, 6 };
		darray<int> expected = { 3, 0, 15, 72, 9, 6 };

		vrange<int> filtered = make_range(data).filter([](int x) {return x % 3 == 0; });

		//std::cout << filtered << "\n";

		CHECK(expected == filtered.begin());
	}

	SECTION("First items filtered", "[filter][first_item]")
	{
		darray<int> data = { 4, 5, 3, 0, 32, 29, 15, 72, 9, 1, 6 };
		darray<int> expected = { 3, 0, 15, 72, 9, 6 };

		vrange<int> filtered = make_range(data).filter([](int x) {return x % 3 == 0; });

		//std::cout << filtered << "\n";

		CHECK(expected == filtered);
	}

	SECTION("Empty", "[filter][empty]")
	{
		vrange<int> empty;
		darray<int> expected {};

		vrange<int> filtered = empty.filter([](int x) {return x % 3 == 0; });

		CHECK(expected == filtered);
	}

	SECTION("stateful filter", "[filter]")
	{
		darray<int> data = { 3, 4, 3, 32, 4, 15, 15, 15, 1, 32 };
		darray<int> expected = { 3, 4, 32, 15, 1 };
		std::set<int> visited;

		vrange<int> filtered = make_range(data).filter([visited](int x)mutable {return visited.insert(x).second; });

		//std::cout << filtered << "\n";

		CHECK(expected == filtered);
	}
}


TEST_CASE_METHOD(ViewTests,  "transform")
{
	SECTION("Transform function", "[transform]")
	{
		darray<int> data = { 3, 70, 27, 14, 9, 42, 43, 1048576 };
		darray<std::string> expected = { "3", "70", "27", "14", "9", "42", "43", "1048576" };
		
		vrange<std::string> transformed = make_range(data).transform([](int x){return std::to_string(x);});

		CHECK(expected == transformed);
	}
}


TEST_CASE_METHOD(ViewTests,  "Cascade transform")
{
	SECTION("Cascade chained transform", "[chain]")
	{
		darray<int> data = { 3, 70, 28, 14, 9, 42, 43, 7340032 };
		darray<std::string> expected = { "10", "4", "2", "6", "1048576" };

		vrange<std::string> transformed =
			make_range(data)
			.filter([](int x) { return (x % 7) == 0; })
			.transform([](int x) { return x / 7; })
			.transform([](int x) { return std::to_string(x); });

		CHECK(expected == transformed);
	}
}