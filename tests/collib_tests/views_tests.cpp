#include "pch-collib-tests.h"

#include "darray.h"
#include "vrange.h"

using namespace coll;

TEST_CASE("filter_view")
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

	//SECTION("Empty", "[filter_view][empty]")
	//{
	//	vrange<int> empty;
	//	darray<int> expected {};

	//	vrange<int> filtered = csl::filter_view<int>(empty.begin(), [](int x) {return x % 3 == 0; });

	//	CHECK(sequence_equals(expected.begin(), filtered.begin()));
	//}
}


//TEST_CASE("transform_view")
//{
//	SECTION("Transform view", "[transform_view]")
//	{
//		darray<int> data = { 3, 70, 27, 14, 9, 42, 43, 1048576 };
//		darray<std::string> expected = { "3", "70", "27", "14", "9", "42", "43", "1048576" };
//		
//		vrange<std::string> transformed = csl::transform_view<int, std::string>(data.begin(), [](int x)->std::string
//			{
//				return std::to_std::string(x);
//			});
//
//		CHECK(sequence_equals(expected.begin(), transformed.begin()));
//	}
//}


//TEST_CASE("Chained transform")
//{
//	SECTION("Chained transform", "[chain_operator]")
//	{
//		darray<int> data = { 3, 70, 28, 14, 9, 42, 43, 7340032 };
//		darray<std::string> expected = { "10", "4", "2", "6", "1048576" };
//
//		vrange<std::string> transformed = 
//			data.begin()
//			| csl::filter_view<int>([](int x) { return (x % 7) == 0; })
//			| csl::transform_view<int, int>([](int x){ return x/7; })
//			| csl::transform_view<int, std::string>([](int x){ return std::to_std::string(x); });
//
//		CHECK(sequence_equals(expected.begin(), transformed.begin()));
//	}
//}

TEST_CASE("filter")
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


TEST_CASE("transform")
{
	SECTION("Transform function", "[transform]")
	{
		darray<int> data = { 3, 70, 27, 14, 9, 42, 43, 1048576 };
		darray<std::string> expected = { "3", "70", "27", "14", "9", "42", "43", "1048576" };
		
		vrange<std::string> transformed = make_range(data).transform([](int x){return std::to_string(x);});

		CHECK(expected == transformed);
	}
}


TEST_CASE("Cascade transform")
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