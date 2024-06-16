#include "nurbs.h"
#include <geometry/representation/IfcGeometry.h>
#include <geometry/representation/geometry.h>
#include <geometry/operations/geometryutils.h>
#include <tinynurbs/tinynurbs.h>
#include <spdlog/spdlog.h>
#include <CDT.h>
#include <utils/debug/watch.hpp>
#include <numeric>
#include <string>
#include <execution>
#include <iostream>
#include <utils/exports/obj/obj.hpp>

namespace webifc::geometry{

	void Nurbs::fill_geometry(){
		auto uv_points {this->get_uv_points()};
		auto indices {get_triangulation_uv_points(uv_points)};

		// Subdivide resulting triangles to increase definition
		// r indicates the level of subdivision, currently 3 you can increase it to 5
		// for (size_t r = 0; r < 3; r++)
		// {
		// 	auto num_indices{indices.size()};
		// 	std::vector<uint32_t> newIndices;
		// 	newIndices.reserve(num_indices / 3 * 12);
		// 	Nurbs::uv_points_t newUVPoints;
		// 	newUVPoints.reserve(num_indices / 3 * 6);

		// 	for (size_t i = 0; i < num_indices; i += 3)
		// 	{
		// 		auto const& p0 = newUVPoints.emplace_back(std::move(uv_points[indices[i + 0]]));
		// 		auto const& p1 = newUVPoints.emplace_back(std::move(uv_points[indices[i + 1]]));
		// 		auto const& p2 = newUVPoints.emplace_back(std::move(uv_points[indices[i + 2]]));
		// 		newUVPoints.emplace_back((p0.x + p1.x) / 2, (p0.y + p1.y) / 2);
		// 		newUVPoints.emplace_back((p0.x + p2.x) / 2, (p0.y + p2.y) / 2);
		// 		newUVPoints.emplace_back((p1.x + p2.x) / 2, (p1.y + p2.y) / 2);

		// 		int offset = newUVPoints.size() - 6;

		// 		newIndices.push_back(offset + 0);
		// 		newIndices.push_back(offset + 3);
		// 		newIndices.push_back(offset + 4);

		// 		newIndices.push_back(offset + 3);
		// 		newIndices.push_back(offset + 5);
		// 		newIndices.push_back(offset + 4);

		// 		newIndices.push_back(offset + 3);
		// 		newIndices.push_back(offset + 1);
		// 		newIndices.push_back(offset + 5);

		// 		newIndices.push_back(offset + 4);
		// 		newIndices.push_back(offset + 5);
		// 		newIndices.push_back(offset + 2);
		// 	}

		// 	uv_points = newUVPoints;
		// 	indices = newIndices;
		// }

		for (size_t i = 0; i < indices.size(); i += 3)
		{
			auto const& p0 {uv_points[indices[i + 0]]};
			auto const& p1 {uv_points[indices[i + 1]]};
			auto const& p2 {uv_points[indices[i + 2]]};
			auto pt00 {tinynurbs::surfacePoint(*this->nurbs, p0.x, p0.y)};
			auto pt01 {tinynurbs::surfacePoint(*this->nurbs, p1.x, p1.y)};
			auto pt10 {tinynurbs::surfacePoint(*this->nurbs, p2.x, p2.y)};
			geometry.AddFace(pt00, pt01, pt10);
		}
	}	

	Nurbs::Nurbs(IfcGeometry& geometry, std::vector<IfcBound3D>const & bounds, IfcSurface const& surface, double const scaling)
		: geometry{geometry}, 
			bounds{bounds}, 
			bspline_surface{surface.BSplineSurface},
			num_u{this->bspline_surface.ControlPoints.size()},
			num_v{this->bspline_surface.ControlPoints.front().size()},
			scaling{scaling} {
			this->init();
	}
	
	void Nurbs::init(){
		this->nurbs = std::make_shared<tinynurbs::RationalSurface3d>(
				static_cast<int>(this->bspline_surface.UDegree),
				static_cast<int>(this->bspline_surface.VDegree),
				this->get_knots(this->bspline_surface.UKnots, this->bspline_surface.UMultiplicity),
				this->get_knots(this->bspline_surface.VKnots, this->bspline_surface.VMultiplicity),
				tinynurbs::array2<glm::dvec3>{this->num_u, this->num_v, this->get_control_points()},
				tinynurbs::array2<double>{this->num_u, this->num_v, this->get_weights()});
		this->range_knots_u = {
			this->nurbs->knots_u[this->nurbs->degree_u],
			this->nurbs->knots_u[this->nurbs->knots_u.size() - this->nurbs->degree_u - 1]
		};
		this->range_knots_v = {
			this->nurbs->knots_v[this->nurbs->degree_v],
			this->nurbs->knots_v[this->nurbs->knots_v.size() - this->nurbs->degree_v - 1]
		};
		this->ptc = tinynurbs::surfacePoint(*this->nurbs, 0.0, 0.0);
		this->pth = tinynurbs::surfacePoint(*this->nurbs, 1.0, 0.0);
		this->ptv = tinynurbs::surfacePoint(*this->nurbs, 0.0, 1.0);
		this->dh = glm::distance(ptc, pth);
		this->dv = glm::distance(ptc, ptv);
		this->pr = (dh + 1) / (dv + 1);
		this->minError /= this->scaling;
		this->maxError /= this->scaling;
	}
	std::vector<double> Nurbs::get_weights() const{
		std::vector<double> result(this->num_u * this->num_v);
		std::fill(result.begin(), result.end(), 1.0);
		for (auto const& row : this->bspline_surface.Weights)
			for (size_t i{0}; i < row.size(); ++i)
			{
				result[i] = row[i];
		}
		return result;
	}
	std::vector<double> Nurbs::get_knots(std::vector<double>const & bs_knots, std::vector<uint32_t> const & bs_mults) const{
		std::vector<double> result;
		auto knots_no_expanded {this->check_knots(bs_knots)}; 
		auto const num_srf_knots {std::accumulate(bs_mults.begin(), bs_mults.end(), 0.0)};
		result.reserve(num_srf_knots);
		for(size_t knot_i{0}; knot_i < bs_knots.size(); ++knot_i){
			auto const knot {knots_no_expanded[knot_i]};
			auto const knot_mult {bs_mults[knot_i]};
			for(size_t i{0}; i < knot_mult; ++i) result.push_back(knot);
		}
		return result;
	}
	std::vector<glm::dvec3> Nurbs::get_control_points() const{
		std::vector<glm::dvec3> result;
		size_t num_points{0};
		for(auto const& row : this->bspline_surface.ControlPoints) num_points += row.size();
		result.reserve(num_points);
		for(auto const& row : this->bspline_surface.ControlPoints)  std::copy(row.begin(), row.end(), std::back_inserter(result));
		return result;
	}
	Nurbs::uv_points_t Nurbs::get_uv_points() const{
		Nurbs::uv_points_t points;
		auto const& bound_points {this->bounds.front().curve.points};
		size_t num_points{bound_points.size()};
		points.resize(num_points);
		std::cout << std::format("Transform {} points", num_points);
	 	debug::watch watch{};
		std::transform(bound_points.begin(), bound_points.end(), points.begin(), [&](auto const& point){
				auto uv {this->inverse_evaluation(point)};
				return Nurbs::uv_point_t{uv.x, uv.y};
		});
		std::cout << std::format(" in {} ms\n", watch.get_time_ms());
		std::sort(points.begin(), points.end(),[](auto const& left, auto const& right){
			  if (left[0] != right[0]) {
          return left[0] < right[0];
        }
        return left[1] < right[1];
		});
		auto last_it2 = std::unique(points.begin(), points.end(), [](auto const& a, auto const& b){
				double EPS{1E-5};
				return std::abs(a[0] - b[0]) < EPS && std::abs(a[1] - b[1]) < EPS;
		});
		points.erase(last_it2, points.end());
		std::sort(points.begin(), points.end(),[](auto const& left, auto const& right){
		  return left[1] < right[1];
		});
		this->dump_uv_points(points);
		return points;
	}
	auto Nurbs::get_approximation(glm::dvec3 const& pt, uv_point_t const& range_u, uv_point_t const& range_v) const{
		double fU{0.0};
		double fV{0.0};
    int const grid_size {10};
    auto min_distance = std::numeric_limits<double>::max();
		auto const portion_u {std::abs((range_u.y - range_u.x) / grid_size)};
		auto const portion_v {std::abs((range_v.y - range_v.x) / grid_size)};
    auto const middle_step_u {portion_u / 2};
    auto const middle_step_v {portion_v / 2};
		auto new_range_u {range_u};
		auto new_range_v {range_v};
		for (int i = 0; i <= grid_size; ++i) {
				auto const step_u {portion_u * i + middle_step_u};
				auto const u {range_u.x + step_u};
        for (int j = 0; j <= grid_size; ++j) {
					auto const step_v {portion_v * i + middle_step_v};
					auto const v {range_v.x + step_v};
					auto const pt_grid {tinynurbs::surfacePoint(*this->nurbs, u, v)};
					auto const dist {glm::distance(pt_grid, pt)};
					if (dist < min_distance) {
							min_distance = dist;
							fU = u;
							fV = v;
							new_range_u = {range_u.x + portion_u * i, range_u.x + portion_u * (i + 1) };
							new_range_v = {range_v.x + portion_v * i, range_v.x + portion_v * (i + 1) };
					}
        }
    }
		return std::tuple {min_distance, fU, fV, new_range_u, new_range_v};
	}	
	Nurbs::uv_point_t Nurbs::inverse_evaluation(glm::dvec3 const& pt) const
	{
		spdlog::debug("[BSplineInverseEvaluation({})]");
		return inverse_method(pt);
	}
	Nurbs::uv_point_t Nurbs::inverse_method(glm::dvec3 const& pt) const
	{
		spdlog::debug("[InverseMethod({})]");
		glm::highp_dvec3 pt00{};
		auto max_distance = std::numeric_limits<double>::max();
		double fU {0.5};
		double fV {0.5};
		// auto [max_distance, fU, fV, new_range_u, new_range_v] {this->get_approximation(pt, this->range_knots_u, this->range_knots_v)};
		// auto previous_distance{max_distance};
		// while(max_distance > maxError){
		// 	auto [next_max_distance, next_fU, next_fV, range_u, range_v] {this->get_approximation(pt, new_range_u, new_range_v)};
		// 	fU = next_fU;
		// 	fV = next_fV;
		// 	if(max_distance <= maxError) return {fU, fV};
		// 	if((previous_distance - next_max_distance) < 0.10) break;
		// 	previous_distance = next_max_distance;
		// 	new_range_u = range_u;
		// 	new_range_v = range_v;
		// 	max_distance = next_max_distance;
		// }
		
		size_t count{0};
		double divisor {10.0};
		while (max_distance > maxError && divisor < 10000)
		{
			for (double r = 1; r < 5; r++)
			{
				int round = 0;
				auto mul_divisor {r * r * divisor};
				while (max_distance > minError && round < 3)
				{
					for (double i = 0; i < rotations; i++)
					{
						double rads = (i / rotations) * pi2;
						double incU = glm::sin(rads) / mul_divisor;
						double incV = glm::cos(rads) / mul_divisor;
						if (pr > 1) incV *= pr;
						else incU /= pr;
						while (true)
						{
							++count;
							// spdlog::debug("result: {} \t || {} \t || count: {} \t",fU, fV, count);
							double ffU = fU + incU;
							double ffV = fV + incV;
							if (ffU < range_knots_u.x)
							{
									ffU = range_knots_u.y - (range_knots_u.x - ffU);
							}
							else if (ffU > range_knots_u.y)
							{
									ffU = range_knots_u.x + (ffU - range_knots_u.y);
							}
							if (ffV < range_knots_v.x)
							{
									ffV = range_knots_v.y - (range_knots_v.x - ffV);
							}
							else if (ffV > range_knots_v.y)
							{
									ffV = range_knots_v.x + (ffV - range_knots_v.y);
							}	

							pt00 = tinynurbs::surfacePoint(*this->nurbs, ffU, ffV);
							auto const di {glm::distance(pt00, pt)};
							if (di < max_distance)
							{
								max_distance = di;
								fU = ffU;
								fV = ffV;
							}
							else
							{
								break;
							}
						}
					}
					round++;
				}
			}
			divisor *= 3;
		}
		return {fU, fV};
	}
	std::vector<uint32_t> Nurbs::get_triangulation_uv_points(Nurbs::uv_points_t const& uv_points) const{
		std::vector<uint32_t> result;
		if(uv_points.empty()) return result;
		auto const num_points {uv_points.size()};
		auto const num_edges {num_points};
		CDT::Triangulation<double> triangulator{CDT::VertexInsertionOrder::Auto};
		std::vector<CDT::V2d<double>> points;
		points.resize(num_points);
		std::transform(uv_points.begin(), uv_points.end(), points.begin(), [](auto const& uv_point){
			return CDT::V2d<double>{uv_point.x, uv_point.y};
		});

		try
		{
			triangulator.insertVertices(points);
			triangulator.eraseSuperTriangle();
			auto const num_indices{triangulator.triangles.size() * 3};
			result.reserve(triangulator.triangles.size() * 3);
			for(auto const& triangle : triangulator.triangles){
				auto & vertice0_id{triangle.vertices[0]};
				auto & vertice1_id{triangle.vertices[1]};
				auto & vertice2_id{triangle.vertices[2]};
				auto vertice0 {glm::dvec2{uv_points[vertice0_id][0], uv_points[vertice0_id][1]}};
				auto vertice1 {glm::dvec2{uv_points[vertice1_id][0], uv_points[vertice1_id][1]}};
				auto vertice2 {glm::dvec2{uv_points[vertice2_id][0], uv_points[vertice2_id][1]}};
				auto area{areaOfTriangle(vertice0, vertice1, vertice2)};
				constexpr double EPS {1E-3};
				constexpr double EPS2 {EPS*EPS};
				if(area < EPS2) {
					continue;
				}
				result.emplace_back(std::move(vertice0_id));
				result.emplace_back(std::move(vertice1_id));
				result.emplace_back(std::move(vertice2_id));
			}
		}
		catch(...){ return {};}
		return result;
	}
	std::vector<double> Nurbs::get_zscores(std::vector<double> const& knots) const{
		std::vector<double> result(knots.size());
		double mean = std::accumulate(knots.begin(), knots.end(), 0.0) / knots.size();
		double sq_sum = std::inner_product(knots.begin(), knots.end(), knots.begin(), 0.0);
		double stdev = std::sqrt(sq_sum / knots.size() - mean * mean);
		for (size_t i = 0; i < knots.size(); ++i) {
				result[i] = (knots[i] - mean) / stdev;
		}
		return result;
	}
	std::vector<double> Nurbs::check_knots(std::vector<double> const& knots) const{
		std::vector<double> result(knots.size());
		auto const num_knots {knots.size()};
		if(num_knots == 2){
			result[0] = 0.0;
			result[1] = 1.0;
			return result;
		}
		auto threshold {3.0};
		auto zscores {get_zscores(knots)};
		for(size_t i{0}; i < num_knots; ++i){
			if(std::abs(zscores[i]) > threshold){
				if(i == 0)									result[i] = knots[i+1];
				else if (i == num_knots -1)	result[i] = knots[i-1];
				else 												result[i] = (knots[i-1]+knots[i+1]) / 2.0;
			}
			else result[i] = knots[i]; 
		}
		return result;
	}
	void Nurbs::dump_uv_points(uv_points_t const& uv_points) const{
		auto num_points{uv_points.size()};
		utils::exports::data_obj obj_data_uv{};
		obj_data_uv.lengths_axis = vector_t{10.0, 10.0, 10.0}; 
		obj_data_uv.vertices.reserve(num_points);
		obj_data_uv.material_file = "materials";	
		obj_data_uv.show_id = true;
		for(size_t i {1}; i < uv_points.size(); ++i){
			auto const& bs1 {uv_points[i-1]};
			auto const& bs0 {uv_points[i-0]};
			if(i==1) obj_data_uv.vertices.emplace_back(bs0[0], bs0[1], 0.0); 
			auto& line_uv {obj_data_uv.lines.emplace_back()};
			line_uv.material = i;
			line_uv.indexes.push_back(i);
			obj_data_uv.vertices.emplace_back(bs1[0], bs1[1], 0.0);
			line_uv.indexes.push_back(i+1);
		}
		utils::exports::obj::write_obj(L"exports/objs/uv_BSpline.obj", obj_data_uv);
	}
}