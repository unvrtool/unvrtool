//
// Copyright (c) 2020, Jan Ove Haaland, all rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <opencv2/opencv.hpp>

#include "util_cv.hpp"

#include "camera.hpp"
#include "shader.hpp"
#include "geometry.hpp"


class GlRenderTarget
{
public:
	enum class Type { RGB8, Uv16 };
	Type type;
	int renderWidth = 1280;
	int renderHeight = 720;
	cv::Mat dumpbuf;
	cv::Mat renderImg;
	Shader* shader;
	unsigned int framebuffer;
	Config::Rgb backColor;

	void Init(Shader& s, Type rendertype, int width, int height)
	{
		shader = &s;
		type = rendertype;
		renderWidth = width;
		renderHeight = height;

		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

		// create a color attachment texture
		unsigned int textureColorbuffer;
		glGenTextures(1, &textureColorbuffer);
		glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
		if (type == Type::RGB8)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, renderWidth, renderHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
		if (type == Type::Uv16)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG, renderWidth, renderHeight, 0, GL_RG, GL_UNSIGNED_SHORT, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Not used, but needed?
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);
		// glViewport  set size of drawing area

		// create a renderbuffer object for depth attachment
		unsigned int depthrenderbuffer;
		glGenRenderbuffers(1, &depthrenderbuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, renderWidth, renderHeight); // use a single renderbuffer object for depth 
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer); // now actually attach it

		//// Set the list of draw buffers.
		//GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
		//glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

		// now that we actually created the framebuffer and added all attachments we want to check if it is actually complete now
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;

		// bind to framebuffer and draw scene as we normally would to color texture 
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void SetSize(int width, int height)
	{
		renderWidth = width;
		renderHeight = height;
	}

	void Draw(int texture1, Camera& cam, Geometry& geom)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		shader->use();
		glViewport(0, 0, renderWidth, renderHeight);

		if (type == Type::RGB8)
			glClearColor(backColor.r, backColor.g, backColor.b, 1.0f);
		if (type == Type::Uv16)
			glClearColor(0, 0, 0, 1);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture1);

		glm::mat4 projection = glm::perspective(glm::radians(cam.fov()), (renderWidth / (float)renderHeight), 0.1f, 100.0f);
		shader->setMat4("projection", projection);
		glm::mat4 view = cam.CalcView();
		shader->setMat4("view", view);
		geom.GlDraw();

		//std::fstream dumpbuf_fd("c:/git/LearnOpenGL-master/fbodump.rgb", std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
		//dumpbuf_fd.write((char*)dumpbuf, scrWidth * scrHeight * 3);
		{
			int size = renderWidth * renderHeight * 3;
			if (type == Type::Uv16)
				size = renderWidth * renderHeight * 4;

			glReadBuffer(GL_COLOR_ATTACHMENT0);
			if (type == Type::RGB8)
			{
				ucv::Ensure(dumpbuf, renderHeight, renderWidth, CV_8UC3);

				glReadPixels(0, 0, renderWidth, renderHeight, GL_BGR, GL_UNSIGNED_BYTE, dumpbuf.data);
				cv::flip(dumpbuf, renderImg, 0);
			}
			if (type == Type::Uv16)
			{
				ucv::Ensure(renderImg, renderHeight, renderWidth, CV_16UC2);
				ucv::Ensure(dumpbuf, renderHeight, renderWidth, CV_32SC1); // Flip doesn't like CV_16UC2, so pretend to be CV_32SC1 instead
				cv::Mat renderImgx(renderHeight, renderWidth, CV_32SC1, (void*)renderImg.data);

				glReadPixels(0, 0, renderWidth, renderHeight, GL_RG, GL_UNSIGNED_SHORT, dumpbuf.data);
				cv::flip(dumpbuf, renderImgx, 0);
				assert(renderImg.data == renderImgx.data);
			}
		}

		// ----------------
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
};

