#include "return_codes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZLIB)
#include <zlib.h>
#elif defined(LIBDEFLATE)
#include <libdeflate.h>
#else
#error Invalid lib
#endif

const unsigned char chunkIHDR[] = { 'I', 'H', 'D', 'R' };
const unsigned char chunkIDAT[] = { 'I', 'D', 'A', 'T' };
const unsigned char chunkPLTE[] = { 'P', 'L', 'T', 'E' };
const unsigned char chunkIEND[] = { 'I', 'E', 'N', 'D' };
const unsigned char chunkbKGD[] = { 'b', 'K', 'G', 'D' };
const unsigned char chunktRNS[] = { 't', 'R', 'N', 'S' };
const unsigned char pdfSignature[] = { 137, 'P', 'N', 'G', '\r', '\n', '\x1A', '\n' };

struct imgInf
{
	int isError;
	size_t width;
	size_t height;
	int colorType;
};

struct imgData
{
	int isError;
	unsigned char *arr;
	unsigned char *palette;
	size_t lengthArr;
	int paletteLength;
	unsigned char p;
	unsigned char background[3];
	char hasAlphaColor;
	unsigned char alphaColor[3];
	unsigned char *alphaType3;
};

int comparison(const unsigned char *c1, const unsigned char *c2, int n)
{
	for (int i = 0; i < n; i++)
	{
		if (c1[i] != c2[i])
		{
			return 0;
		}
	}
	return 1;
}

int getNumber(unsigned char *c)
{
	return c[3] + c[2] * 256 + c[1] * 256 * 256 + c[0] * 256 * 256 * 256;
}

struct imgInf decodeIHDR(FILE *in)
{
	struct imgInf result;
	unsigned char buffer[9];
	result.isError = ERROR_DATA_INVALID;
	if (fread(buffer, 1, 4, in) != 4)
	{
		return result;
	}
	if (getNumber(buffer) != 13 || fread(buffer, 1, 4, in) != 4 || !comparison(buffer, chunkIHDR, 4))
	{
		return result;
	}
	if (fread(buffer, 1, 4, in) != 4)
	{
		return result;
	}
	result.width = getNumber(buffer);
	if (fread(buffer, 1, 4, in) != 4)
	{
		return result;
	}
	result.height = getNumber(buffer);
	if (fread(buffer, 1, 9, in) != 9)
	{
		return result;
	}
	result.colorType = buffer[1];
	if ((result.colorType != 0 && result.colorType != 2 && result.colorType != 3 && result.colorType != 4 && result.colorType != 6) ||
		buffer[0] != 8 || buffer[2] != 0 || buffer[3] != 0 || buffer[4] != 0)
	{
		return result;
	}
	result.isError = SUCCESS;
	return result;
}

char checkType(const unsigned char *arr, int n)
{
	for (int i = 0; i < n / 3; i++)
	{
		if (arr[(i * 3)] != arr[(i * 3) + 1] || arr[(i * 3)] != arr[(i * 3) + 2] || arr[(i * 3) + 1] != arr[(i * 3) + 2])
		{
			return '6';
		}
	}
	return '5';
}

struct imgData decodeImg(FILE *in, int type)
{
	unsigned char buffer[4];
	unsigned char name[4];
	struct imgData result;
	result.hasAlphaColor = 0;
	result.isError = SUCCESS;
	result.paletteLength = 0;
	result.background[0] = 0;
	result.background[1] = 0;
	result.background[2] = 0;
	unsigned char *arr;
	int boolean = 1;
	int l, count;
	result.lengthArr = 0;
	while (fread(buffer, 1, 4, in) != 0)
	{
		count = 0;
		l = getNumber(buffer);
		fread(name, 1, 4, in);
		if (comparison(chunkPLTE, name, 4))
		{
			if (type != 3 || l % 3 != 0)
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			if (!(result.palette = malloc(l)))
			{
				result.isError = ERROR_OUT_OF_MEMORY;
				return result;
			}
			if (!(result.alphaType3 = malloc(l / 3)))
			{
				result.isError = ERROR_OUT_OF_MEMORY;
				return result;
			}
			for (int i = 0; i < l / 3; i++)
			{
				result.alphaType3[i] = 255;
			}
			count += (fread(result.palette, 1, l, in));
			result.paletteLength = l;
			result.p = checkType(result.palette, result.paletteLength);
			count += (fread(buffer, 1, 4, in));
			if (count != l + 4)
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			continue;
		}
		if (comparison(chunkbKGD, name, 4))
		{
			if (type == 3 && l == 1)
			{
				count += fread(result.background, 1, 1, in);
			}
			else if (l == 2 || l == 6)
			{
				for (int i = 0; i < l / 2; i++)
				{
					count += fread(buffer, 1, 2, in);
					result.background[i] = buffer[1];
				}
			}
			else
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			count += fread(buffer, 1, 4, in);
			if (count != l + 4)
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			continue;
		}
		if (comparison(chunktRNS, name, 4))
		{
			if (type == 4 || type == 6 || (type == 3 && result.paletteLength < l * 3) || (type == 0 && l != 1) ||
				(type == 2 && l != 3))
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			if (type == 3)
			{
				count += fread(result.alphaType3, 1, l, in);
			}
			else
			{
				result.hasAlphaColor = 1;
				count += fread(result.alphaColor, 1, l, in);
			}
			count += fread(buffer, 1, 4, in);
			if (count != l + 4)
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			continue;
		}
		if (!comparison(chunkIDAT, name, 4))
		{
			if (!(arr = malloc(l + 4)))
			{
				result.isError = ERROR_OUT_OF_MEMORY;
				return result;
			}
			count += (fread(arr, 1, l + 4, in));
			free(arr);
			if (count != l + 4)
			{
				result.isError = ERROR_DATA_INVALID;
				return result;
			}
			continue;
		}
		if (boolean)
		{
			boolean = 0;
			if (!(result.arr = malloc(l)))
			{
				result.isError = ERROR_OUT_OF_MEMORY;
				return result;
			}
		}
		else
		{
			arr = realloc(result.arr, l + result.lengthArr);
			if (!arr)
			{
				result.isError = ERROR_OUT_OF_MEMORY;
				return result;
			}
			result.arr = arr;
		}
		count += (fread(result.arr + result.lengthArr, 1, l, in));
		result.lengthArr += l;
		count += (fread(buffer, 1, 4, in));
		if (count != l + 4)
		{
			free(arr);
			result.isError = ERROR_DATA_INVALID;
			return result;
		}
	}
	if (!comparison(name, chunkIEND, 4) || l != 0 || fread(buffer, 1, 1, in) != 0)
	{
		result.isError = ERROR_DATA_INVALID;
	}
	return result;
}

void unfilter(size_t width, size_t height, int type, int isAlpha, unsigned char *arr)
{
	size_t count = 0;
	int left, up, lu, a, b, c;
	int k = ((type == 6) ? 3 : 1) + isAlpha;
	for (size_t i = 0; i < height; i++)
	{
		count++;
		switch (arr[count - 1])
		{
		case (0):
			count += width * k;
			break;
		case (1):
			for (int j = 0; j < width * k; j++)
			{
				left = 0;
				if (j >= k)
				{
					left = arr[count - k];
				}
				arr[count] += left;
				count++;
			}
			break;
		case (2):
			for (int j = 0; j < width * k; j++)
			{
				up = 0;
				if (i > 0)
				{
					up = arr[count - width * k - 1];
				}
				arr[count] += up;
				count++;
			}
			break;
		case (3):
			for (int j = 0; j < width * k; j++)
			{
				left = 0;
				if (j >= k)
				{
					left = arr[count - k];
				}
				up = 0;
				if (i > 0)
				{
					up = arr[count - width * k - 1];
				}
				arr[count] += (left + up) / 2;
				count++;
			}
			break;
		case (4):
			for (int j = 0; j < width * k; j++)
			{
				left = 0;
				if (j >= k)
				{
					left = arr[count - k];
				}
				up = 0;
				if (i > 0)
				{
					up = arr[count - width * k - 1];
				}
				lu = 0;
				if (j >= k && i > 0)
				{
					lu = arr[count - width * k - 1 - k];
				}
				int p = left + up - lu;
				a = abs(p - left);
				b = abs(p - up);
				c = abs(p - lu);
				if (a <= b && a <= c)
				{
					arr[count] += left;
				}
				else if (b <= c)
				{
					arr[count] += up;
				}
				else
				{
					arr[count] += lu;
				}
				count++;
			}
			break;
		}
	}
}

int main(int argc, char **argv)
{
	if (!(argc == 3 || argc == 4))
	{
		fprintf(stderr, "The parameter or number of parameters (argv) is incorrect");
		return ERROR_PARAMETER_INVALID;
	}
	FILE *in = fopen(argv[1], "rb");
	if (in == NULL)
	{
		fprintf(stderr, "File can't be opened");
		return ERROR_CANNOT_OPEN_FILE;
	}
	unsigned char buffer[8];
	if (fread(buffer, 1, 8, in) != 8 || !comparison(buffer, pdfSignature, 8))
	{
		fprintf(stderr, "The data is invalid");
		return ERROR_DATA_INVALID;
	}

	struct imgInf inf = decodeIHDR(in);
	if (inf.isError == ERROR_DATA_INVALID)
	{
		fprintf(stderr, "The data is invalid");
		return inf.isError;
	}

	struct imgData data = decodeImg(in, inf.colorType);
	fclose(in);
	if (data.isError != SUCCESS)
	{
		if (data.lengthArr > 0)
		{
			free(data.arr);
		}
		if (data.paletteLength > 0)
		{
			free(data.palette);
			free(data.alphaType3);
		}
		if (data.isError == ERROR_DATA_INVALID)
		{
			fprintf(stderr, "The data is invalid");
		}
		else
		{
			fprintf(stderr, "Not enough memory");
		}
		return data.isError;
	}

	unsigned long long length;
	int type;
	if (inf.colorType != 3)
	{
		if (inf.colorType == 0 || inf.colorType == 4)
		{
			type = 5;
		}
		else
		{
			type = 6;
		}
	}
	else
	{
		type = data.p;
	}
	length = inf.height * (inf.width * (((type == 6) ? 3 : 1) + ((inf.colorType == 4 || inf.colorType == 6) ? 1 : 0)) + 1);
	unsigned char *res;
	if (!(res = (unsigned char *)malloc(length)))
	{
		free(data.arr);
		if (data.paletteLength > 0)
		{
			free(data.palette);
			free(data.alphaType3);
		}
		fprintf(stderr, "Not enough memory");
		return ERROR_OUT_OF_MEMORY;
	}
	int errorCode;
#if defined(ZLIB)
	errorCode = uncompress(res, (uLongf *)&length, data.arr, data.lengthArr);
	if (errorCode != Z_OK)
	{
		free(data.arr);
		if (data.paletteLength > 0)
		{
			free(data.palette);
			free(data.alphaType3);
		}
		free(res);
		if (errorCode == Z_MEM_ERROR)
		{
			fprintf(stderr, "Not enough memory");
			return ERROR_OUT_OF_MEMORY;
		}
		else
		{
			fprintf(stderr, "The data is invalid");
			return ERROR_DATA_INVALID;
		}
	}
#elif defined(LIBDEFLATE)
	struct libdeflate_decompressor *d = libdeflate_alloc_decompressor();
	errorCode = libdeflate_zlib_decompress(d, data.arr, data.lengthArr, res, length, NULL);
	libdeflate_free_decompressor(d);
	if (errorCode != LIBDEFLATE_SUCCESS)
	{
		free(data.arr);
		if (data.paletteLength > 0)
		{
			free(data.palette);
			free(data.alphaType3);
		}
		free(res);
		if (errorCode == LIBDEFLATE_BAD_DATA)
		{
			fprintf(stderr, "The data is invalid");
			return ERROR_DATA_INVALID;
		}
		else
		{
			fprintf(stderr, "Not enough memory");
			return ERROR_OUT_OF_MEMORY;
		}
	}
#endif
	free(data.arr);
	unfilter(inf.width, inf.height, type, ((inf.colorType == 4 || inf.colorType == 6) ? 1 : 0), res);

	FILE *out = fopen(argv[2], "wb");
	if (out == NULL)
	{
		if (data.paletteLength > 0)
		{
			free(data.palette);
			free(data.alphaType3);
		}
		free(res);
		fprintf(stderr, "File can't be opened");
		return ERROR_CANNOT_OPEN_FILE;
	}
	if (argc == 4)
	{
		for (int i = 0; i < 3; i++)
		{
			data.background[i] = 0;
		}
		if (strlen(argv[3]) > 3)
		{
			int j = 0;
			for (int i = 0; i < strlen(argv[3]); i++)
			{
				if (argv[3][i] == ' ')
				{
					j++;
					continue;
				}
				data.background[j] *= 10;
				data.background[j] += (argv[3][i] - '0');
			}
		}
		else
		{
			for (int i = 0; i < strlen(argv[3]); i++)
			{
				data.background[0] *= 10;
				data.background[0] += (argv[3][i] - '0');
			}
			data.background[1] = data.background[0];
			data.background[2] = data.background[0];
			if (data.paletteLength > 0)
			{
				if (data.paletteLength >= data.background[0] * 3)
				{
					data.background[0] = data.palette[data.background[0] * 3];
					data.background[1] = data.palette[data.background[0] * 3 + 1];
					data.background[2] = data.palette[data.background[0] * 3 + 2];
				}
				else
				{
					data.background[0] = data.palette[0];
					data.background[1] = data.palette[1];
					data.background[2] = data.palette[2];
				}
			}
		}
	}

	size_t count;
	if (inf.colorType == 3)
	{
		if (data.paletteLength == 0)
		{
			free(data.alphaType3);
			free(res);
			fprintf(stderr, "The data is invalid");
			return ERROR_DATA_INVALID;
		}
		fprintf(out, "P%c\n%zu %zu\n255\n", data.p, inf.width, inf.height);
		count = 0;

		for (size_t i = 0; i < inf.height; i++)
		{
			count++;
			for (size_t j = 0; j < inf.width; j++)
			{
				if (data.p == '5')
				{
					int buf = (data.alphaType3[res[count]] * data.palette[res[count] * 3] +
							   (255 - data.alphaType3[res[count]]) * data.background[0]) /
							  255;
					fprintf(out, "%c", buf);
					count++;
				}
				else
				{
					for (int k = 0; k < 3; k++)
					{
						int buf = (data.alphaType3[res[count]] * data.palette[res[count] * 3 + k] +
								   (255 - data.alphaType3[res[count]]) * data.background[k]) /
								  255;
						fprintf(out, "%c", buf);
					}
					count++;
				}
			}
		}
	}
	else
	{
		fprintf(out, "P%c\n%zu %zu\n255\n", (type == 5 ? '5' : '6'), inf.width, inf.height);
		count = 0;
		for (size_t i = 0; i < inf.height; i++)
		{
			count++;
			for (size_t j = 0; j < inf.width; j++)
			{
				if (inf.colorType == 0)
				{
					if (data.hasAlphaColor && data.alphaColor[0] == res[count])
					{
						fprintf(out, "%c", data.background[0]);
					}
					else
					{
						fprintf(out, "%c", res[count]);
					}
					count++;
				}
				else if (inf.colorType == 2)
				{
					if (data.hasAlphaColor && data.alphaColor[0] == res[count] &&
						data.alphaColor[1] == res[count + 1] && data.alphaColor[2] == res[count + 2])
					{
						fprintf(out, "%c%c%c", data.background[0], data.background[1], data.background[2]);
					}
					else
					{
						fprintf(out, "%c%c%c", res[count], res[count + 1], res[count + 2]);
					}
					count += 3;
				}
				else if (inf.colorType == 4)
				{
					int buf = (res[count + 1] * res[count] + (255 - res[count + 1]) * data.background[0]) / 255;
					fprintf(out, "%c", (char)buf);
					count += 2;
				}
				else if (inf.colorType == 6)
				{
					int buf;
					for (int k = 0; k < 3; k++)
					{
						buf = (res[count + 3] * res[count + k] + (255 - res[count + 3]) * data.background[k]) / 255;
						fprintf(out, "%c", (char)buf);
					}
					count += 4;
				}
			}
		}
	}
	if (data.paletteLength > 0)
	{
		free(data.palette);
		free(data.alphaType3);
	}
	free(res);
	if (count != length)
	{
		fprintf(stderr, "The data is invalid");
		return ERROR_DATA_INVALID;
	}
	fclose(out);
	return SUCCESS;
}
