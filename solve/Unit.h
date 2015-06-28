#include <iostream>
#include <chrono>
#include <ratio>

// Experimental no frills units library inspired on Barton & Nackman's Dimensional Analysis (Ch 16.5)
// for dimension handling and std::chrono library / std::ratio for units of measurement.

// A minimal subset of

namespace
{
	// Todo: I could use std::common_type
	// + which is already defined for built-ins (X,Y)
	// + and overloading for Dimension would be to replace the existing Dimension operators (?)

	template <typename X, typename Y>
	using AddType = decltype(std::declval<X>() + std::declval<Y>());

	template <typename X, typename Y>
	using MulType = decltype(std::declval<X>() * std::declval<Y>());

	template <typename D1, typename D2>
	using DivType = decltype(std::declval<D1>() / std::declval<D2>());
}


namespace nufl {

template <int D1, int D2=0, int D3=0>
struct Dimension {
	static constexpr int d1 = D1;
	static constexpr int d2 = D2;
	static constexpr int d3 = D3;
};

template <int m1, int l1, int t1, int m2, int l2, int t2>
Dimension<m1+m2, l1+l2, t1+t2> operator*(Dimension<m1, l1, t1> lhs,
	                                     Dimension<m2, l2, t2> rhs) {
	return Dimension<m1+m2, l1+l2, t1+t2>();
}

template <int a1, int a2, int a3, int b1, int b2, int b3>
using DimMultiply = Dimension<a1+b1, a2+b2, a3+b3>;

template <int m1, int l1, int t1, int m2, int l2, int t2>
Dimension<m1-m2, l1-l2, t1-t2> operator/(Dimension<m1, l1, t1> lhs,
	                                     Dimension<m2, l2, t2> rhs) {
	return Dimension<m1-m2, l1-l2, t1-t2>();
}

template <int D1, int D2, int D3>
Dimension<D1, D2, D3> operator+(Dimension<D1, D2, D3> lhs,
	                            Dimension<D1, D2, D3> rhs) {
	return Dimension<D1, D2, D3>();
}

// Later: rename Base
template <typename Dim = Dimension<1>,
          typename R1 = std::ratio<1>, typename R2 = std::ratio<1>, typename R3 = std::ratio<1>>
struct Scale {
	using dim = Dim;
	using r1 = R1;
	using r2 = R2;
	using r3 = R3;
};


template <typename S1, typename S2>
using IsMultiple = std::integral_constant<bool, (S1::r1::num * S2::r1::den) % (S2::r1::num * S1::r1::den) == 0 &&
                                                (S1::r2::num * S2::r2::den) % (S2::r2::num * S1::r2::den) == 0 &&
                                                (S1::r3::num * S2::r3::den) % (S2::r3::num * S1::r3::den) == 0 >;

template <typename T, typename S>
class Unit;

constexpr int64_t ipow(int64_t base, int exp, int64_t result = 1) {
  return exp < 1 ? result : ipow(base*base, exp/2, (exp % 2) ? result*base : result);
}

template <typename R, int p>
using inv_ratio_power = std::ratio<ipow(R::den,p), ipow(R::num,p)>;

template <typename R, typename Target, int p>
using ConversionRatio = std::ratio_multiply<R, inv_ratio_power<Target, p>>;

template <typename ToUnit, typename X, typename S1, typename D = typename S1::dim>
ToUnit dimension_cast(const Unit<X,S1>& unit)
{
	using Y = typename ToUnit::rep;
	using S = typename ToUnit::scale;

	using conversion = std::ratio_multiply<ConversionRatio<typename S1::r1, typename S::r1, D::d1>,
	                   std::ratio_multiply<ConversionRatio<typename S1::r2, typename S::r2, D::d2>,
	                                       ConversionRatio<typename S1::r3, typename S::r3, D::d3>>>;

	return ToUnit(static_cast<Y>(unit.value()) * conversion::num / conversion::den);
}

template <typename ToUnit, typename X, typename S1>
ToUnit unit_cast(const Unit<X,S1>& unit)
{
	// todo: static_assert()
	using S = typename ToUnit::scale;
	return dimension_cast<ToUnit,X,S1,AddType<typename S1::dim,typename S::dim>>(unit);
}

template <typename T, typename S = Scale<>>
class Unit
{
public:
	using rep = T;
	using scale = S;

	Unit(const T& val) : value_(val) {}

	// Notice no case covers integral T but floating-point X
	template < typename X, typename S1,
		typename std::enable_if_t<
		    std::is_integral<T>::value &&
		    std::is_integral<X>::value &&
			IsMultiple<S1,S>::value, int > = 0 >
	Unit(const Unit<X,S1>& rhs) {
		// New value is target unit / this ratio, which enable_if guarantees to divide evenly
		value_ = rhs.value() * S1::r1::num * S::r1::den / (S1::r1::den * S::r1::num);
	}

	// The seemingly redundant `std::is_floating_point<X>::value` seems required to
	// make this overload conditionally dependent on X (and avoid error while instantiating Unit<T>)
	template < typename X, typename S1,
		typename std::enable_if_t<
		    (std::is_floating_point<T>::value && std::is_floating_point<X>::value) ||
		    (std::is_floating_point<T>::value && !std::is_floating_point<X>::value), int> = 0 >
	Unit(const Unit<X,S1>& rhs) {
		// New value is target unit / this ratio, which enable_if guarantees to divide evenly
		value_ = rhs.value() * static_cast<T>(S1::r1::num * S::r1::den) / static_cast<T>(S1::r1::den * S::r1::num);
	}

	T& value() { return value_; }
	const T& value() const { return value_; }

	template <typename Q = Unit<T,S>>
	Q as() const { return unit_cast<Q>(*this); }

	template <typename Q = Unit<T,S>>
	T asVal() const { return unit_cast<Q>(*this).value(); }

	Unit& operator+=(const Unit& rhs) { value_ += rhs.value(); return *this; }
	Unit& operator-=(const Unit& rhs) { value_ -= rhs.value(); return *this; }
	template <typename X>
	Unit& operator*=(const X& x) { value_ *= x; return *this; }
	template <typename X>
	Unit& operator/=(const X& x) { value_ /= x; return *this; }

	friend std::ostream& operator<<(std::ostream& os, const Unit& q)
	{
		return os << q.value()
		       << " (" << scale::r1::num << "/" << scale::r1::den
		       << ", " << scale::r2::num << "/" << scale::r2::den
		       << ", " << scale::r3::num << "/" << scale::r3::den
		       << ")"
		       << " [" << scale::dim::d1 << "," << scale::dim::d2 << "," << scale::dim::d3 << "]";

		// std::vector<std::string> bases;
		// bases.push_back(std::to_string(scale::r1::num) + "/" + std::to_string(scale::r1::den));
		// bases.push_back(std::to_string(scale::r2::num) + "/" + std::to_string(scale::r2::den));
		// bases.push_back(std::to_string(scale::r3::num) + "/" + std::to_string(scale::r3::den));

		// std::vector<std::string> active;
		// for (int d=1; d<3; d++) {
		// 	if (d) { active.push_back(bases[d]); }
		// }

		// os << q.value();
		// if (active.begin() != active.end())
		// {
		// 	os << " (" << *active.begin();
		// 	for (auto iter = active.begin() + 1; iter != active.end(); iter++) {
		// 		os << ", " << *iter;
		// 	}
		// 	os << ")";
		// }

		// return os << " [" << Dim::d1 << "," << Dim::d2 << "," << Dim::d3 << "]";;
	}

private:
	T value_;
};

template <typename R1, typename R2>
using CommonRatio = typename std::common_type<std::chrono::duration<int,R1>, std::chrono::duration<int,R2>>::type::period;

template <typename D, typename S1, typename S2>
using CommonScale = Scale<D, CommonRatio<typename S1::r1 , typename S2::r1>,
                             CommonRatio<typename S1::r2 , typename S2::r2>,
                             CommonRatio<typename S1::r3 , typename S2::r3>>;

// Unit + - * / Unit

template <typename X, typename Y, typename S1, typename S2,
          typename ToUnit = Unit< AddType<X,Y>, CommonScale<AddType<typename S1::dim,typename S2::dim>,S1,S2>> >
ToUnit operator+(const Unit<X,S1>& lhs, const Unit<Y,S2>& rhs)
{
	using S = typename ToUnit::scale;
	return ToUnit(unit_cast<Unit<X,S>>(lhs).value() + unit_cast<Unit<Y,S>>(rhs).value());
}

template <typename X, typename Y, typename S1, typename S2,
          typename ToUnit = Unit< AddType<X,Y>, CommonScale<AddType<typename S1::dim,typename S2::dim>,S1,S2>> >
ToUnit operator-(const Unit<X,S1>& lhs, const Unit<Y,S2>& rhs)
{
	using S = typename ToUnit::scale;
	return ToUnit(unit_cast<Unit<X,S>>(lhs).value() - unit_cast<Unit<Y,S>>(rhs).value());
}

template <typename X, typename Y, typename S1, typename S2,
          typename ToUnit = Unit< AddType<X,Y>, CommonScale<MulType<typename S1::dim,typename S2::dim>,S1,S2>> >
ToUnit operator*(const Unit<X,S1>& lhs, const Unit<Y,S2>& rhs)
{
	using S = typename ToUnit::scale;
	return ToUnit(dimension_cast<Unit<X,S>>(lhs).value() * dimension_cast<Unit<Y,S>>(rhs).value());
}

template <typename X, typename Y, typename S1, typename S2,
          typename ToUnit = Unit< AddType<X,Y>, CommonScale<DivType<typename S1::dim,typename S2::dim>,S1,S2>> >
ToUnit operator/(const Unit<X,S1>& lhs, const Unit<Y,S2>& rhs)
{
	using S = typename ToUnit::scale;
	return ToUnit(dimension_cast<Unit<X,S>>(lhs).value() / dimension_cast<Unit<Y,S>>(rhs).value());
}

// Todo: see `TEST(UnitTest, DivType)`
template <typename X, typename Y, typename S>
MulType<X,Y> operator/(const Unit<X,S>& lhs, const Unit<Y,S>& rhs)
{
	return MulType<X,Y>(lhs.value() / rhs.value());
}


// // Scalar * * / Unit

template <typename X, typename Y, typename S,
          typename = std::enable_if_t<std::is_arithmetic<Y>::value>>
Unit<MulType<X,Y>,S> operator*(const Unit<X,S>& lhs, const Y& y)
{
	return Unit<MulType<X,Y>,S>(lhs.value() * y);
}

template <typename X, typename Y, typename S,
          typename = std::enable_if_t<std::is_arithmetic<Y>::value>>
Unit<MulType<X,Y>,S> operator*(const Y& y, const Unit<X,S>& rhs)
{
	return Unit<MulType<X,Y>,S>(rhs.value() * y);
}

template <typename X, typename Y, typename S,
          typename = std::enable_if_t<std::is_arithmetic<Y>::value>>
Unit<MulType<X,Y>,S> operator/(const Unit<X,S>& lhs, const Y& y)
{
	return Unit<MulType<X,Y>,S>(lhs.value() / y);
}



// Helper types

namespace si {

// Base dimensions


template <typename r = std::ratio<1>> using Length = Scale<Dimension<1>, r>;
template <typename r = std::ratio<1>> using Length2 = Scale<Dimension<2>, r>;
template <typename r = std::ratio<1>> using Time = Scale<Dimension<0,1>, r>;
template <typename r = std::ratio<1>> using Time2 = Scale<Dimension<0,2>, r>;
template <typename r = std::ratio<1>> using Mass = Scale<Dimension<0,0,1>, r>;



// Derived dimensions

template <typename r1, typename r2> using Velocity = Scale<Dimension<1,-1>, r1, r2>;
template <typename r1, typename r2> using Acceleration = Scale<Dimension<1,-2>, r1, r2>;
template <typename r1, typename r2, typename r3> using Force = Scale<Dimension<1,1,-2>, r1, r2, r3>;


// Useful ratios

using meter = std::ratio<1>;
using inch = std::ratio<39>;
using hour = std::ratio<3600>;


// Base units

using Meter = Unit<float, Length<meter>>;
using Meter2 = Unit<float, Length2<meter>>;
using Centimeter = Unit<float, Length<std::centi>>;
using Centimeter2 = Unit<float, Length2<std::centi>>;
using Millimeter = Unit<float, Length<std::milli>>;

using Inch = Unit<float, Length<std::ratio<39>>>;

template <int n, int d>
using BaseRatio = Scale<Dimension<1>, std::ratio<n,d>>;

// Derived units

using m_s = Unit<float, Velocity<meter, meter>>;
using in_hr = Unit<float, Velocity<inch, hour>>;


} // si

std::ostream& operator<<(std::ostream& os, const si::Meter& q)
{ return os << q.value() << " m"; }
std::ostream& operator<<(std::ostream& os, const si::Centimeter& q)
{ return os << q.value() << " cm"; }
std::ostream& operator<<(std::ostream& os, const si::Millimeter& q)
{ return os << q.value() << " mm"; }

std::ostream& operator<<(std::ostream& os, const si::Meter2& q)
{ return os << q.value() << " m^2"; }
std::ostream& operator<<(std::ostream& os, const si::Centimeter2& q)
{ return os << q.value() << " cm^2"; }

} // nufl
