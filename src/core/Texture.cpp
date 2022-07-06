#include "Texture.h"
#include "Buffer.h"

#include <stdio.h>

#include <stdlib.h>

#include <iostream>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>

// perhaps I should use a proper system for taking into account already loaded textures but this will do fine, just for now
//std::map<std::string, GLuint> PreloadedTextureList;

struct CacheLoad {
	bool Successful_;
	unsigned char* Data_;

	operator unsigned char* () {
		return Data_;
	}

	void Free() {
		if (Successful_) {
			delete[] Data_;
		}
		else {
			SOIL_free_image_data(Data_);
		}
	}
};

CacheLoad LoadFromCache(std::string Path, int& Width, int& Height, int& Channels, int force_channels) {
	for (char& c : Path) {
		if (c == '\\') {
			c = '/';
		}
	}

	constexpr bool ClearCache = false;

	CacheLoad NewLoad;

	std::string CachedPath = "cache/" + Path + ".BIN";
	FILE* CacheRead = fopen(CachedPath.c_str(), "rb");

	if (ClearCache && CacheRead) {
		fclose(CacheRead);
		remove(CachedPath.c_str());
		CacheRead = nullptr;
	}

	if (!CacheRead) {
		NewLoad.Successful_ = false;
		
		NewLoad.Data_ = SOIL_load_image(Path.c_str(), &Width, &Height, &Channels, force_channels);

		std::filesystem::create_directories(CachedPath.substr(0, CachedPath.rfind('/')));
		FILE* CreateCache = fopen(CachedPath.c_str(), "wb");

		int MetaData[] = { Width, Height, force_channels };
		fwrite(MetaData, 1, sizeof(MetaData), CreateCache);
		fwrite(NewLoad.Data_, 1, Width * Height * force_channels, CreateCache);

		fclose(CreateCache);
	}
	else {
		NewLoad.Successful_ = true;

		int MetaData[3];
		fread(MetaData, sizeof(MetaData), 1, CacheRead);

		Width = MetaData[0];
		Height = MetaData[1];
		Channels = MetaData[2];

		std::cout << Width << ' ' << Height << ' ' << Channels << '\n';

		long long Size = Width * Height * Channels;
		NewLoad.Data_ = new unsigned char[Size];
		fread(NewLoad.Data_, 1, Size, CacheRead);
		fclose(CacheRead);
	}

	return NewLoad;
}

Texture::Texture() : texture(UINT32_MAX) {

}

GLuint Texture::GetHandle() {
	return texture;
}

GLuint64 Texture::MakeBindless() {
	GLuint64 bindlessHandle = glGetTextureHandleARB(GetHandle());
	glMakeTextureHandleResidentARB(bindlessHandle);
	return bindlessHandle;
}

void Texture2D::SetColor(const vec4& color) {
	LoadData(GL_RGBA32F, GL_RGBA, GL_FLOAT, 1, 1, (void*)&color.r);
	SaveData(GL_FLOAT, 1, 1, (void*)&color.r);
}

void Texture2D::SetColor(const vec3& color) {
	SetColor(vec4(color, 1.0f));
}

void Texture::EnsureGeneratedHandle() {
	if (texture == UINT32_MAX) {
		glGenTextures(1, &texture);
	}
}

void Texture::Free() {
	glDeleteTextures(1, &texture);
}

void Texture2D::CreateBinding() {
	EnsureGeneratedHandle();
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture::BindImageUnit(uint32_t Unit, GLenum Format) {
	glBindImageTexture(Unit, texture, 0, GL_FALSE, 0, GL_READ_WRITE, Format);
}

void Texture::BindTextureUnit(uint32_t unit, GLenum target) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, texture);
}

void Texture2D::FreeBinding() {
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::LoadTexture(const char* Path, int load) {

	int Width = 0;
	int Height = 0;
	int Channels = 0;

	CacheLoad TextureData = LoadFromCache(Path, Width, Height, Channels, load);

	LoadData(GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, Width, Height, TextureData);
	SaveData(GL_UNSIGNED_BYTE, Width, Height, TextureData);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void Texture2D::LoadTexture(const std::string& Path, int load) {
	LoadTexture(Path.c_str(), load);
}

void Texture2D::LoadData(GLenum DestinationFormat, GLenum SourceFormat, GLenum SourceType, uint32_t X, uint32_t Y, void* Data) {
	glTexImage2D(GL_TEXTURE_2D, 0, DestinationFormat, X, Y, 0, SourceFormat, SourceType, Data);
}

void Texture2D::SaveData(GLenum SourceType, uint32_t X, uint32_t Y, void* Data) {
	width = X;
	height = Y;

	if (SourceType == GL_UNSIGNED_BYTE)
		imagei = new uint8_t[3ULL * width * height];
	else
		imagef = new float[3ULL * width * height];

	for (uint32_t i = 0; i < height; i++) {
		for (uint32_t j = 0; j < width; j++) {
			int idx = i * width + j;
			int didx = 3 * idx;
			int sidx = 4 * idx;
			if (SourceType == GL_UNSIGNED_BYTE) {
				uint8_t* source = (uint8_t*)Data;
				imagei[didx    ] = source[sidx    ];
				imagei[didx + 1] = source[sidx + 1];
				imagei[didx + 2] = source[sidx + 2];
			}
			else {
				vec4* source = (vec4*)Data;
				imagef[didx    ] = source[idx].r;
				imagef[didx + 1] = source[idx].g;
				imagef[didx + 2] = source[idx].b;
			}
		}
	}
	internalFormat = SourceType;
}

vec3 Texture2D::Sample(const vec2 texcoords) const {
	// Cast to integer coordinates
	ivec2 texelcoords = texcoords * vec2(width, height);
	texelcoords.x %= width;
	texelcoords.y %= height;
	uint32_t idx = 3 * (texelcoords.y * width + texelcoords.x);
	// Nearest neighbor sampling
	return (internalFormat == GL_UNSIGNED_BYTE ? vec3(imagei[idx], imagei[idx+1], imagei[idx+2]) / 255.0f : vec3(imagef[idx], imagef[idx+1], imagef[idx+2]));
}

void TextureBuffer::CreateBinding() {
	EnsureGeneratedHandle();
	glBindTexture(GL_TEXTURE_BUFFER, texture);
}

void TextureBuffer::FreeBinding() {
	glBindTexture(GL_TEXTURE_BUFFER, 0);
}

void TextureBuffer::SelectBuffer(Buffer* Buf, GLenum Format, uint32_t DataOffset, uint32_t DataBytes) {
	ReferencedBuffer = Buf;

	Offset = DataOffset;
	Bytes = DataBytes;

	glTexBufferRange(GL_TEXTURE_BUFFER, Format, ReferencedBuffer->BufferHandle, Offset, Bytes);
}

void TextureBuffer::SelectBuffer(Buffer* Buf, GLenum Format) {
	ReferencedBuffer = Buf;

	glTexBuffer(GL_TEXTURE_BUFFER, Format, ReferencedBuffer->BufferHandle);
}

void TextureCubemap::CreateBinding() {
	EnsureGeneratedHandle();
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
}

void TextureCubemap::FreeBinding() {
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void TextureCubemap::LoadTexture(const std::string& wpath) {
	CreateBinding();

	std::string path = wpath;
	for (char& c : path)
		if (c == '\\')
			c = '/';

	std::string folder = path.substr(0, path.find_last_of('/') + 1);;
	std::ifstream locations(path);

	for (int i = 0; i < 6; i++) {
		std::string facePath;
		std::getline(locations, facePath);
		if (facePath.front() == '#') {
			i--;
			continue;
		}

		facePath = folder + facePath;

		int width, height, nrChannels;
		unsigned char* face = SOIL_load_image(facePath.c_str(), &width, &height, &nrChannels, SOIL_LOAD_RGBA);


		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, face);
		faces[i].SaveData(GL_UNSIGNED_BYTE, width, height, face);
		SOIL_free_image_data(face);
	}
	

}

vec3 TextureCubemap::Sample(vec3 texcoords) const {
	// Taken from wikipedia https://en.wikipedia.org/wiki/Cube_mapping#Memory_addressing
	texcoords.y = -texcoords.y;

	int index;

	float absX = fabs(texcoords.x);
	float absY = fabs(texcoords.y);
	float absZ = fabs(texcoords.z);

	int isXPositive = texcoords.x > 0 ? 1 : 0;
	int isYPositive = texcoords.y > 0 ? 1 : 0;
	int isZPositive = texcoords.z > 0 ? 1 : 0;

	float maxAxis, uc, vc;

	// POSITIVE X
	if (isXPositive && absX >= absY && absX >= absZ) {
		// u (0 to 1) goes from +z to -z
		// v (0 to 1) goes from -y to +y
		maxAxis = absX;
		uc = -texcoords.z;
		vc = texcoords.y;
		index = 0;
	}
	// NEGATIVE X
	if (!isXPositive && absX >= absY && absX >= absZ) {
		// u (0 to 1) goes from -z to +z
		// v (0 to 1) goes from -y to +y
		maxAxis = absX;
		uc = texcoords.z;
		vc = texcoords.y;
		index = 1;
	}
	// POSITIVE Y
	if (isYPositive && absY >= absX && absY >= absZ) {
		// u (0 to 1) goes from -x to +x
		// v (0 to 1) goes from +z to -z
		maxAxis = absY;
		uc = texcoords.x;
		vc = -texcoords.z;
		index = 3;
	}
	// NEGATIVE Y
	if (!isYPositive && absY >= absX && absY >= absZ) {
		// u (0 to 1) goes from -x to +x
		// v (0 to 1) goes from -z to +z
		maxAxis = absY;
		uc = texcoords.x;
		vc = texcoords.z;
		index = 2;
	}
	// POSITIVE Z
	if (isZPositive && absZ >= absX && absZ >= absY) {
		// u (0 to 1) goes from -x to +x
		// v (0 to 1) goes from -y to +y
		maxAxis = absZ;
		uc = texcoords.x;
		vc = texcoords.y;
		index = 4;
	}
	// NEGATIVE Z
	if (!isZPositive && absZ >= absX && absZ >= absY) {
		// u (0 to 1) goes from +x to -x
		// v (0 to 1) goes from -y to +y
		maxAxis = absZ;
		uc = -texcoords.x;
		vc = texcoords.y;
		index = 5;
	}

	vec2 uv;

	// Convert range from -1 to 1 to 0 to 1
	uv.x = 0.5f * (uc / maxAxis + 1.0f);
	uv.y = 0.5f * (vc / maxAxis + 1.0f);

	return faces[index].Sample(uv);
}

Texture2D& TextureCubemap::GetFace(uint32_t i) {
	return faces[i];
}