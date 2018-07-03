#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "benchmark.h"
#include "graphics.h"
#include "math.h"

#include <iostream>
#include <vector>
#include <functional>
#include <array>
#include <string>

void save(const graphics::color_buffer& image, std::ostream& stream)
{
	// http://paulbourke.net/dataformats/tga/
	const char header[18] = {
		0,
		0,
		0x02,
		0, 0,
		0, 0,
		0,
		0, 0,
		0, 0,
		static_cast<char>(image.width()) & 0xFF,	static_cast<char>(image.width() >> 8) & 0xFF,
		static_cast<char>(image.height()) & 0xFF, static_cast<char>(image.height() >> 8) & 0xFF,
		24,
		0x20
	};

	stream.write(header, sizeof(header));

	for (int y = 0; y < image.height(); ++y)
	{
		for (int x = 0; x < image.width(); ++x)
		{
			const graphics::color& c = image.get(x, y);

			stream.put(c.b);
			stream.put(c.g);
			stream.put(c.r);
		}
	}
}

class debug_stringbuf : public std::stringbuf
{
public:
	~debug_stringbuf()
	{
		sync(); // can be avoided
	}

	virtual int sync() override
	{
		OutputDebugString(str().c_str());
		str("");
		return 0;
	}
};

struct model
{
	std::vector<graphics::mesh> _meshes;
};

bool load_model(model& model, const char* path)
{
	std::vector<graphics::mesh> meshes;

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, path);
	if (!ret)
	{
		std::cerr << err << std::endl;

		return false;
	}

	// Only support one shape
	assert(shapes.size() == 1);
	size_t shape_index = 0;

	graphics::mesh mesh;

	size_t index_offset = 0;
	for (size_t face_index = 0; face_index < shapes[shape_index].mesh.num_face_vertices.size(); face_index++)
	{
		const int num_face_vertices = shapes[shape_index].mesh.num_face_vertices[face_index];

		assert(num_face_vertices == 3);

		tinyobj::index_t idx0 = shapes[shape_index].mesh.indices[index_offset + 0];
		tinyobj::index_t idx1 = shapes[shape_index].mesh.indices[index_offset + 1];
		tinyobj::index_t idx2 = shapes[shape_index].mesh.indices[index_offset + 2];

		graphics::vertex a;
		graphics::vertex b;
		graphics::vertex c;

		a.position = {
			(attrib.vertices[3 * idx2.vertex_index + 0]),
			(attrib.vertices[3 * idx2.vertex_index + 1]),
			(attrib.vertices[3 * idx2.vertex_index + 2])
		};

		b.position = {
			(attrib.vertices[3 * idx1.vertex_index + 0]),
			(attrib.vertices[3 * idx1.vertex_index + 1]),
			(attrib.vertices[3 * idx1.vertex_index + 2])
		};

		c.position = {
			(attrib.vertices[3 * idx0.vertex_index + 0]),
			(attrib.vertices[3 * idx0.vertex_index + 1]),
			(attrib.vertices[3 * idx0.vertex_index + 2])
		};

		a.normal = {
			attrib.normals[3 * idx2.normal_index + 0],
			attrib.normals[3 * idx2.normal_index + 1],
			attrib.normals[3 * idx2.normal_index + 2],
		};

		b.normal = {
			attrib.normals[3 * idx1.normal_index + 0],
			attrib.normals[3 * idx1.normal_index + 1],
			attrib.normals[3 * idx1.normal_index + 2],
		};

		c.normal = {
			attrib.normals[3 * idx0.normal_index + 0],
			attrib.normals[3 * idx0.normal_index + 1],
			attrib.normals[3 * idx0.normal_index + 2],
		};

		mesh.triangles.push_back({ a, b, c });

		index_offset += num_face_vertices;
	}

	model._meshes.push_back(mesh);

	return true;
}

int main()
{
	const int resolution_x = 640;
	const int resolution_y = 480;

	model model;
	bool model_loaded = load_model(model, "models/teapot_with_normals.obj");
	for (auto& mesh : model._meshes)
	{
		mesh.transform = math::create_rotation_x(math::pi / 2);
		mesh.transform = math::multiply(mesh.transform, math::create_rotation_y(-math::pi / 5));
		math::set_translation(mesh.transform, math::vec<3>(0.0f, 6.0f, 0.0f));
	}

	if (!model_loaded)
	{
		std::cerr << "load_model: Fail" << std::endl;

		return 1;
	}

	// Set up a left-handed projection (+z points into the screen)
	math::mat<4> camera_transform = math::create_identity<4>();
	math::set_translation(camera_transform, math::get_forward(camera_transform) * -100.0f);

	math::vec<3> at = math::get_translation(camera_transform) + math::get_forward(camera_transform);
	math::vec<3> eye = math::get_translation(camera_transform);
	math::vec<3> up = math::get_up(camera_transform);
	math::mat<4> view_matrix = math::create_look_at_lh(
		at,
		eye,
		up);
	math::mat<4> proj_matrix = math::create_perspective_fov_lh(
		math::pi / 10,
		static_cast<float>(resolution_x) / resolution_y,
		1.0f,
		2048.0f);

	graphics::color_buffer buffer(resolution_x, resolution_y);
	for (int i = 0; i < 100; ++i)
	{
		benchmark::benchmark p("frame");

		graphics::submit(buffer, view_matrix, proj_matrix, model._meshes);
	}

	std::ofstream result_file("frame.tga", std::ofstream::binary);
	save(buffer, result_file);

	std::ofstream perf_file("frame.txt");
	benchmark::benchmark::report(std::cout);
	benchmark::benchmark::report(std::ostream(&debug_stringbuf()));
	benchmark::benchmark::report(perf_file);

	return 0;
}

#if 0
int main()
{
	const int window_width = 640;
	const int window_height = 480;

	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::cerr << "SDL_Init: " << SDL_GetError() << std::endl;

		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Hello World!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_SHOWN);
	if (!window) 
	{
		std::cerr << "SDL_CreateWindow: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return 1;
	}

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
	if (!renderer) 
	{
		std::cerr << "SDL_CreateRenderer: " << SDL_GetError() << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	model model;
#if DRAW_TEAPOT
	bool model_loaded = load_model(model, "models/teapot.obj", 4, 50);
#else
	bool model_loaded = load_model(model, "models/diablo3_pose/diablo3_pose.obj", 1, 200);
#endif

	if (!model_loaded)
	{
		std::cerr << "load_model: Fail"  << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();
		return 1;
	}

	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, window_width, window_height);

	bool quit = false;
	while (!quit)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				quit = true;
				break;
			}
		}

		graphics::image buffer(window_width, window_height);
		{
			benchmark::benchmark p("draw");

			graphics::draw(buffer, model._triangles);
		}

		SDL_RenderClear(renderer);
		SDL_UpdateTexture(texture, nullptr, buffer.ptr(), buffer.pitch());
		SDL_RenderCopy(renderer, texture, nullptr, nullptr);
		SDL_RenderPresent(renderer);
	}

	benchmark::benchmark::report(std::cout);
	benchmark::benchmark::report(std::ostream(&debug_stringbuf()));

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
#endif