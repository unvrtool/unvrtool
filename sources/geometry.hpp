//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "vrimageformat.hpp"

class Geometry
{
public:
	struct xyzyv
	{
		float x=0, y=0, z=0, u=0, v=0;
		xyzyv() {}
		xyzyv(float px, float py, float pz, float tu, float tv) { x = px; y = py; z = pz; u = tu; v = tv; }

		float UvDist2(glm::vec2 uv) { return (u - uv.x) * (u - uv.x) + (v - uv.y) * (v - uv.y); }
	};

	VrImageGeometryMapping::Type geomMappingType = VrImageGeometryMapping::Type::Equirectangular;
	float planeAspectRatio = 1;
	float FovX = 360;
	float FovY = 180;
	int Nx = 256;
	int Ny = 128;
	cv::Rect2f fisheyeEllipseRect;
	
	float FovRadX() { return util::rad(FovX); }
	float FovRadY() { return util::rad(FovY); }

	std::vector<xyzyv> pts;
	std::vector<xyzyv> verticesVec;

	int numRects;
	bool voAllocated = false;
	unsigned int VBO=0, VAO=0;

	void DeleteVo()
	{
		if (voAllocated)
		{
			voAllocated = false;
			glDeleteVertexArrays(1, &VAO);
			glDeleteBuffers(1, &VBO);
		}
	}

	void AllocVo()
	{
		DeleteVo();
		voAllocated = true;
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
	}


	~Geometry()
	{
		DeleteVo();
	}

	void Set(VrImageGeometryMapping::Type type, float fovX, float fovY = 180, float pointsPerDeg = 1)
	{
		geomMappingType = type;
		FovX = fovX;
		FovY = fovY;
		planeAspectRatio = fovX / fovY;
		Nx = (int)roundf(FovX / pointsPerDeg);
		Ny = (int)roundf(FovY / pointsPerDeg);
	}

	glm::vec2 CalcTexFromYawPitch(YawPitch p)
	{
		const float pi = util::pi;
		float tx = 0.5f + p.Yaw / FovX;
		float ty = 0.5f + p.Pitch / FovY;

		if (geomMappingType == VrImageGeometryMapping::Type::Fisheye)
		{
			float rx = p.Yaw * pi / 180;
			float ry = p.Pitch * pi / 180;
			float R = glm::cos(ry);
			float py = -glm::sin(ry);
			float pz = R * glm::cos(rx);
			float px = -R * glm::sin(rx);

			float pxy = sqrtf(px * px + py * py);
			float a = atan2(pxy, pz);
			float r = sinf(a);
			float sc = 2 * a / (pi * r);
			tx = (-px * sc + 1) / 2;
			ty = (-py * sc + 1) / 2;
			tx = fisheyeEllipseRect.x + tx * fisheyeEllipseRect.width;
			ty = fisheyeEllipseRect.y + ty * fisheyeEllipseRect.height;
		}

		return glm::vec2(tx, ty);
	}

	
	void GeneratePlanePoints()
	{
		Ny = Nx = 2;
		int Pts = Ny * Nx;
		pts.clear();
		pts.reserve(Pts);
		auto x = planeAspectRatio;
		pts.emplace_back( x,  1.f, 1.f, 0.f, 0.f);
		pts.emplace_back(-x,  1.f, 1.f, 1.f, 0.f);
		pts.emplace_back( x, -1.f, 1.f, 0.f, 1.f);
		pts.emplace_back(-x, -1.f, 1.f, 1.f, 1.f);
	}

	void GenerateSpherePoints()
	{
		const float pi = util::pi;
		int Pts = Ny * Nx;
		pts.clear();
		pts.reserve(Pts);
		float iNx = 1.0f / (Nx - 1);
		float iNy = 1.0f / (Ny - 1);
		float rfx = util::rad(FovX) / 2;
		float rfy = util::rad(FovY) / 2;

		for (int y = 0; y < Ny; y++)
			for (int x = 0; x < Nx; x++)
			{
				float tx = x * iNx; // 0-1
				float ty = y * iNy;
				float rx = (tx * 2 - 1) * rfx;
				float ry = (ty * 2 - 1) * rfy;

				float R = glm::cos(ry);
				float py = -glm::sin(ry);
				float pz = R * glm::cos(rx);
				float px = -R * glm::sin(rx);

				if (geomMappingType == VrImageGeometryMapping::Type::Fisheye)
				{
					// r = k sin(a)  Orthographic (orthogonal / Sine-law)
					// https://en.wikipedia.org/wiki/Fisheye_lens, http://michel.thoby.free.fr/Fisheye_history_short/Projections/Models_of_classical_projections.html
					float pxy = sqrtf(px * px + py * py);
					float a = atan2(pxy, pz);
					float r = sinf(a);
					float sc = 2 * a / (pi * r);
					tx = (-px* sc + 1) / 2;
					ty = (-py* sc + 1) / 2;
					// Offset for finding fisheye texture in uploaded texture
					tx = fisheyeEllipseRect.x + tx * fisheyeEllipseRect.width;
					ty = fisheyeEllipseRect.y + ty * fisheyeEllipseRect.height;
				}

				pts.emplace_back(px, py, pz, tx, ty);
			}
	}

	void GeneratePoints()
	{
		if (geomMappingType == VrImageGeometryMapping::Type::Flat)
			GeneratePlanePoints();
		else
			GenerateSpherePoints();
	}

	glm::vec3 Tex2Dir(float u, float v)
	{
		auto N = pts.size();
		glm::vec2 uv(u, v);
		int ib = 0;
		float db = 0;
		for (int i=0; i<N; i++)
		{
			float d = pts[i].UvDist2(uv);
			if (i == 0 || d < db)
			{
				ib = i;
				db = d;
			}
		}

		float el = ib - 1 < 0 ? 9 : pts[ib - 1].UvDist2(uv);
		float er = ib + 1 >= N ? 9 : pts[ib + 1].UvDist2(uv);
		float eu = ib - Nx < 0 ? 9 : pts[ib - Nx].UvDist2(uv);
		float ed = ib + Nx >= N ? 9 : pts[ib + Nx].UvDist2(uv);
		int ix = el < er ? -1 : 1;
		int iy = eu < ed ? -Nx : Nx;
		auto& p = pts[ib];
		auto& px = pts[ib + ix];
		auto& py = pts[ib + iy];
		float rx = (u - p.u) / (px.u - p.u);
		float ry = (v - p.v) / (py.v - p.v);
		float x = (1 - rx) * p.x + rx * px.x;
		float y = (1 - ry) * p.y + ry * py.y;
		float z = 1 - (x * x + y * y);
		z = z > 0 ? sqrt(z) : 0;

		return glm::vec3(x, y, z);
	}

	void GenerateVerts()
	{
		GeneratePoints();

		numRects = (Nx - 1) * (Ny - 1);
		verticesVec.clear();
		verticesVec.reserve(numRects * 6);

		int vi = 0;
		for (int y = 0; y < Ny - 1; y++)
			for (int x = 0; x < Nx - 1; x++)
			{
				verticesVec.push_back(pts[y * Nx + x]);
				verticesVec.push_back(pts[y * Nx + x + 1]);
				verticesVec.push_back(pts[(y + 1) * Nx + x + 1]);

				verticesVec.push_back(pts[(y + 1) * Nx + x + 1]);
				verticesVec.push_back(pts[(y + 1) * Nx + x]);
				verticesVec.push_back(pts[y * Nx + x]);
			}
	}

	void GlGenerate()
	{
		GenerateVerts();

		float* vertices = (float*)verticesVec.data();
		int sizeof_vertices = numRects * 6 * 5 * sizeof(float);

		AllocVo();
		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof_vertices, vertices, GL_STATIC_DRAW);

		// position attribute
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);
		// texture coord attribute
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
	}

	void GlDraw()
	{
		glBindVertexArray(VAO);
		glDrawArrays(GL_TRIANGLES, 0, 2 * 3 * numRects);
	}
};

