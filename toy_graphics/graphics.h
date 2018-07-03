#pragma once

#include <vector>
#include <array>

#include "math.h"

namespace graphics
{
#define LINE_MODE 0

	struct color
	{
		static const color red;
		static const color green;
		static const color blue;
		static const color black;
		static const color white;

		uint8_t b, g, r, a;
	};

	const color color::red = { 0, 0, 255, 255 };
	const color color::green = { 0, 255, 0, 255 };
	const color color::blue = { 255, 0, 0, 255 };
	const color color::black = { 0, 0, 0, 255 };
	const color color::white = { 255, 255, 255, 255 };

	class color_buffer
	{
	public:
		color_buffer(int width, int height)
		{
			_width = width;
			_height = height;

			_data.resize(_width * _height);
		}

		void set(int x, int y, const color& color)
		{
			_data[(_width * y) + x] = color;
		}

		const color& get(int x, int y) const
		{
			return _data[(_width * y) + x];
		}

		void clear()
		{
			std::memset(&_data[0], 255, _width * _height * sizeof(color));
		}

		const void* ptr() const
		{
			return &(_data[0]);
		}

		int width() const
		{
			return _width;
		}

		int height() const
		{
			return _height;
		}

		int pitch() const
		{
			return _width * sizeof(color);
		}

	private:
		int _width;
		int _height;
		std::vector<color> _data;
	};

	struct vertex
	{
		math::vec<3> position;
		math::vec<3> normal;
	};

	struct triangle
	{
		vertex a, b, c;
	};

	struct mesh
	{
		math::mat<4> transform;
		std::vector<triangle> triangles;
	};

#if LINE_MODE
	void rasterize_line(color_buffer& buffer, const std::array<int, 2>& a, const std::array<int, 2>& b, const color& color)
	{
		const int dx = abs(b[0] - a[0]);
		const int sx = a[0] < b[0] ? 1 : -1;
		const int dy = abs(b[1] - a[1]);
		const int sy = a[1] < b[1] ? 1 : -1;

		int error = (dx > dy ? dx : -dy) / 2;

		std::array<int, 2> p = a;

		for (;;)
		{
			buffer.set(p[0], p[1], color);

			if (p == b)
			{
				break;
			}

			const int temp = error;
			if (temp > -dx)
			{
				error -= dy;
				p[0] += sx;
			}
			if (temp < dy)
			{
				error += dx;
				p[1] += sy;
			}
		}
	}
#endif

	// Determine which side of the vector b - a does c lie
	bool edge(const math::vec<2>& a, const math::vec<2>& b, const math::vec<2>& c)
	{
		// 2x2 determinant
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x) >= 0;
	}

	math::vec<3> barycentric(const math::vec<2>& p, const math::vec<2>& a, const math::vec<2>& b, const math::vec<2>& c)
	{
		math::vec<2> v0 = b - a;
		math::vec<2> v1 = c - a;
		math::vec<2> v2 = p - a;

		float d00 = math::dot(v0, v0);
		float d01 = math::dot(v0, v1);
		float d11 = math::dot(v1, v1);
		float d20 = math::dot(v2, v0);
		float d21 = math::dot(v2, v1);

		float denom = d00 * d11 - d01 * d01;
		float v = (d11 * d20 - d01 * d21) / denom;
		float w = (d00 * d21 - d01 * d20) / denom;
		float u = 1.0f - v - w;

		return { u, v, w };
	}

	void rasterize_triangle(
		color_buffer& buffer,
		const vertex& a,
		const vertex& b,
		const vertex& c,
		const color& color)
	{
#if LINE_MODE
		rasterize_line(buffer, { static_cast<int>(a.position[0]), static_cast<int>(a.position[1]) }, { static_cast<int>(b.position[0]), static_cast<int>(b.position[1]) }, color);
		rasterize_line(buffer, { static_cast<int>(b.position[0]), static_cast<int>(b.position[1]) }, { static_cast<int>(c.position[0]), static_cast<int>(c.position[1]) }, color);
		rasterize_line(buffer, { static_cast<int>(c.position[0]), static_cast<int>(c.position[1]) }, { static_cast<int>(a.position[0]), static_cast<int>(a.position[1]) }, color);
#else

		const float rf = static_cast<float>(color.r);
		const float gf = static_cast<float>(color.g);
		const float bf = static_cast<float>(color.b);

		std::pair<int, int> bbox_x = std::minmax({ static_cast<int>(a.position.x), static_cast<int>(b.position.x), static_cast<int>(c.position.x ) });
		std::pair<int, int> bbox_y = std::minmax({ static_cast<int>(a.position.y), static_cast<int>(b.position.y), static_cast<int>(c.position.y ) });

		bbox_x.first = std::max(bbox_x.first, 0);
		bbox_y.first = std::max(bbox_y.first, 0);
		bbox_x.second = std::min(bbox_x.second, buffer.width() - 1);
		bbox_y.second = std::min(bbox_y.second, buffer.height() - 1);

		math::vec<3> light_direction = math::normalize<3>({ -1, 0, 1 });

		for (int py = bbox_y.first; py < bbox_y.second + 1; ++py)
		{
			float pyf = static_cast<float>(py);

			for (int px = bbox_x.first; px < bbox_x.second + 1; ++px)
			{
				float pxf = static_cast<float>(px);

				// check if we're inside the triangle
				if (edge(a.position.xy, b.position.xy, { pxf, pyf }) &&
					edge(b.position.xy, c.position.xy, { pxf, pyf }) &&
					edge(c.position.xy, a.position.xy, { pxf, pyf }))
				{
					math::vec<3> bary = barycentric(
						{ pxf, pyf },
						a.position.xy,
						b.position.xy,
						c.position.xy);

					math::vec<3> interpolated_normal = a.normal * bary.x + b.normal * bary.y + c.normal * bary.z;

					//interpolated_normal = math::normalize(interpolated_normal);

					float n_dot_l = std::max(math::dot(light_direction, interpolated_normal), 0.0f);

					buffer.set(px, py,
						{
							static_cast<uint8_t>(bf * n_dot_l),
							static_cast<uint8_t>(gf * n_dot_l),
							static_cast<uint8_t>(rf * n_dot_l),
							255
						});
				}
			}
		}
#endif
	}

	void submit(color_buffer& buffer, const math::mat<4>& view_matrix, const math::mat<4>& proj_matrix, const std::vector<mesh>& meshes)
	{
		const float buffer_width = static_cast<float>(buffer.width());
		const float buffer_height = static_cast<float>(buffer.height());

		buffer.clear();

		const math::mat<4> view_proj = math::multiply(view_matrix, proj_matrix);
		const math::vec<3> camera_forward = { view_matrix[0].z, view_matrix[1].z, view_matrix[2].z };

		for (const mesh& mesh : meshes)
		{
			for (const triangle& triangle : mesh.triangles)
			{
				math::mat<4> world_view_proj = math::multiply(mesh.transform, view_proj);

				vertex a_ss = triangle.a;
				vertex b_ss = triangle.b;
				vertex c_ss = triangle.c;

				// Transform the normal from LS -> WS
				a_ss.normal = math::transform_vector(mesh.transform, a_ss.normal);
				b_ss.normal = math::transform_vector(mesh.transform, b_ss.normal);
				c_ss.normal = math::transform_vector(mesh.transform, c_ss.normal);

				if (math::dot(a_ss.normal, camera_forward) > 0 || math::dot(b_ss.normal, camera_forward) > 0 || math::dot(c_ss.normal, camera_forward) > 0)
				{
					// Transform position from LS -> WS -> VS -> CS -> NDC
					a_ss.position = math::transform_point(world_view_proj, a_ss.position);
					b_ss.position = math::transform_point(world_view_proj, b_ss.position);
					c_ss.position = math::transform_point(world_view_proj, c_ss.position);

					// Then from NDC -> SS
					a_ss.position = ((a_ss.position + 1.0f) / 2) * math::vec<3>(buffer_width, buffer_height, 1.0f);
					b_ss.position = ((b_ss.position + 1.0f) / 2) * math::vec<3>(buffer_width, buffer_height, 1.0f);
					c_ss.position = ((c_ss.position + 1.0f) / 2) * math::vec<3>(buffer_width, buffer_height, 1.0f);

					rasterize_triangle(
						buffer,
						a_ss,
						b_ss,
						c_ss,
						{ 0, 0, 255, 255 });
				}
			}
		}
	}
}