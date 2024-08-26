#include <hal/dma_types.h>
#include <rom/cache.h>
#include <esp_heap_caps.h>

class DMAVideoBuffer
{
	protected:
	int	descriptorCount;
	dma_descriptor_t *descriptors;
	static const int MAX_BUFFERS = 2;
	static const int MAX_LINES = 481;
	void *buffer[MAX_BUFFERS][MAX_LINES];

	public:
	int bufferCount;
	bool valid;
	int lines;
	int lineSize;
	int clones;

	static const int ALIGNMENT_SRAM = 4;

	void attachBuffer(int b = 0)
	{
		for(int i = 0; i < lines; i++) 
			for(int j = 0; j < clones; j++)
				descriptors[i * clones + j].buffer = buffer[b][i];
	}

	uint8_t *getLineAddr8(int y ,int b = 0)
	{
		return (uint8_t*)buffer[b][y];
	}


	uint16_t *getLineAddr16(int y ,int b = 0)
	{
		return (uint16_t*)buffer[b][y];
	}
	
	void scrollTextDown16(int backBuffer)
	{
		buffer[backBuffer][lines] = buffer[backBuffer][0];
		
		for(int y = 0; y < lines; y++)
		{
		buffer[backBuffer][y] = buffer[backBuffer][y + 1];
		}
		
		//attachBuffer(backBuffer);
	}
	

	void scrollFastDown(int backBuffer)  // готово
	{
		buffer[backBuffer][lines] = buffer[backBuffer][0];
		
		for(int y = 0; y < lines; y++)
		{
		buffer[backBuffer][y] = buffer[backBuffer][y + 1];
		}		
		//attachBuffer(backBuffer);
	}
	
	void scrollFastUp(int backBuffer)  // готово
	{
		buffer[backBuffer][0] = buffer[backBuffer][lines - 1];
		
		for(int y = lines - 1; y > 0 ; y--)
		{
		buffer[backBuffer][y] = buffer[backBuffer][y - 1];
		}		
		//attachBuffer(backBuffer);
	}
	
	void scrollFastLeft(int backBuffer)  // доделать
	{

	}
	
	void scrollFastRight(int backBuffer) // доделать
	{

	}

	DMAVideoBuffer(int lines, int lineSize, int clones = 1, bool ring = true, int bufferCount = 1)
	{
		this->lineSize = lineSize;
		this->bufferCount = bufferCount;
		this->lines = lines;
		this->clones = clones;
		valid = false;
		descriptorCount = lines * clones;

		descriptors = (dma_descriptor_t *)heap_caps_aligned_calloc(ALIGNMENT_SRAM, 1, descriptorCount * sizeof(dma_descriptor_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
		if (!descriptors) 
			return;
		for(int i = 0; i < bufferCount; i++)		
			for(int j = 0; j < lines; j++)
				buffer[i][j] = 0;
		for(int i = 0; i < bufferCount; i++)
		{
			for(int y = 0; y < lines; y++)
			{
				buffer[i][y] = heap_caps_aligned_calloc(ALIGNMENT_SRAM, 1, lineSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
				
				if (!buffer[i][y]) 
				{
					return;
				}
			}
		}
		for (int i = 0; i < descriptorCount; i++) 
		{
			descriptors[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
			descriptors[i].dw0.suc_eof = 0;
			descriptors[i].next = &descriptors[i + 1];
			descriptors[i].dw0.size = lineSize;
			descriptors[i].dw0.length = descriptors[i].dw0.size;
		}
		attachBuffer(0);
		if(ring)
			descriptors[descriptorCount - 1].next = descriptors;
		else
		{
			descriptors[descriptorCount - 1].dw0.suc_eof = 1;
			descriptors[descriptorCount - 1].next = 0;
		}
		valid = true;
	}

	~DMAVideoBuffer()
	{
		if(descriptors)
			heap_caps_free(descriptors);
		for(int i = 0; i < bufferCount; i++)		
			for(int j = 0; j < lines; j++)
				if(buffer[i][j])
					heap_caps_free(buffer[i][j]);
	}

	dma_descriptor_t *getDescriptor(int i = 0) const
	{
		return &descriptors[0];
	}

	int getDescriptorCount() const
	{
		return descriptorCount;
	}

	int getLineSize() const
	{
		return lineSize;
	}

	int getBufferCount() const
	{
		return bufferCount;
	}

	bool isValid() const
	{
		return valid;
	}
};