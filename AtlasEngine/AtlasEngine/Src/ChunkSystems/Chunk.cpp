#include "ChunkSystems\Chunk.h"
#include "Rendering\UniformBlockStandard.h"
#include "Debugging\ConsoleOutput.h"
#include "Debugging\DebugText.h"
#include "Rendering\Screen.h"
#include "ChunkSystems\ChunkManager.h"
#include "Physics\PhysicsSystem.h"

FPoolAllocator<sizeof(FBlock) * FChunk::BLOCKS_PER_CHUNK, FChunk::POOL_SIZE> FChunk::ChunkAllocator(__alignof(FChunk));
FPoolAllocatorType<FChunkMesh, FChunk::POOL_SIZE> FChunk::MeshAllocator(__alignof(FChunkMesh));
FPoolAllocatorType<FChunk::CollisionData, FChunk::POOL_SIZE> FChunk::CollisionAllocator(__alignof(FChunk::CollisionData));

int32_t FChunk::BlockIndex(Vector3i Position)
{
	ASSERT(Position.x >= 0 && Position.x < CHUNK_SIZE &&
		Position.y >= 0 && Position.y < CHUNK_SIZE &&
		Position.z >= 0 && Position.z < CHUNK_SIZE);

	const Vector3i PositionToIndex{ CHUNK_SIZE, CHUNK_SIZE * CHUNK_SIZE, 1 };
	return Vector3i::Dot(Position, PositionToIndex);
}

int32_t FChunk::BlockIndex(int32_t X, int32_t Y, int32_t Z)
{
	return FChunk::BlockIndex(Vector3i{ X, Y, Z });
}

FChunk::FChunk()
	: mBlocks(nullptr)
	, mCollisionData(nullptr)
	, mIsLoaded()
	, mIsEmpty()
{
	mIsLoaded = false;
	mIsEmpty = true;

	// Allocate mesh, block, and collision data
	mMesh = new (MeshAllocator.Allocate()) FChunkMesh{};
	mBlocks = static_cast<FBlock*>(ChunkAllocator.Allocate());

	// Construct Collision fields with new memory
	mCollisionData = new (CollisionAllocator.Allocate()) CollisionData{};

	// Set collision data
	CollisionData& CollisionInfo = *mCollisionData;
	CollisionInfo.Mesh.setPremadeAabb(btVector3{ 0, 0, 0 }, btVector3{ 32, 32, 32 });

	// Set collision shape and transform
	CollisionInfo.Object.setCollisionShape(&CollisionInfo.Shape);
}

FChunk::~FChunk()
{
	ChunkAllocator.Free(mBlocks);
	MeshAllocator.Free(mMesh);
	CollisionAllocator.Free(mCollisionData);
}


bool FChunk::Load(const std::vector<uint8_t>& BlockData, const Vector3f& WorldPosition)
{
	ASSERT(!mIsLoaded);

	// Set collision data
	CollisionData& CollisionInfo = *mCollisionData;

	// Set collision transform
	CollisionInfo.Object.setWorldTransform(btTransform{ btQuaternion{0,0,0},
				btVector3{ WorldPosition.x, WorldPosition.y, WorldPosition.z } });

	// Current index to access block type
	int32_t TypeIndex = 0;
	int32_t DataSize = BlockData.size();
	int32_t IsEmpty = 0;

	// Write RLE data for chunk
	for (int32_t y = 0; y < CHUNK_SIZE && TypeIndex < DataSize; y++)
	{
		for (int32_t x = 0; x < CHUNK_SIZE; x++)
		{
			int32_t BaseIndex = BlockIndex(x, y, 0);
			for (int32_t z = 0; z < CHUNK_SIZE;)
			{
				uint8_t Count = 0;
				FBlock::Type BlockType = (FBlock::Type)BlockData[TypeIndex];
				uint8_t RunLength = BlockData[TypeIndex + 1];

				// If a single block is not None, this number can never be 0 again
				IsEmpty += (int32_t)BlockType - (int32_t)FBlock::None;

				for (Count = 0; Count < RunLength; Count++)
				{
					mBlocks[BaseIndex + z + Count].BlockType = BlockType;
				}

				z += RunLength;
				TypeIndex += 2;
			}
		}
	}


	mIsLoaded = true;
	return (IsEmpty == 0);
}

void FChunk::Unload(std::vector<uint8_t>& BlockDataOut)
{
	ASSERT(mIsLoaded);

	mIsLoaded = false;

	// Extract RLE data for chunk
	for (int32_t y = 0; y < CHUNK_SIZE; y++)
	{
		for (int32_t x = 0; x < CHUNK_SIZE; x++)
		{
			for (int32_t z = 0; z < CHUNK_SIZE;)
			{
				const uint32_t CurrentBlockIndex = BlockIndex(x, y, z);
				const FBlock::Type CurrentBlock = mBlocks[CurrentBlockIndex].BlockType;
				uint8_t Length = 1;

				FBlock::Type NextBlock = mBlocks[CurrentBlockIndex + (int32_t)Length].BlockType;
				while (CurrentBlock == NextBlock && Length + z < CHUNK_SIZE)
				{
					Length++;
					NextBlock = mBlocks[CurrentBlockIndex + (int32_t)Length].BlockType;
				}

				// Append RLE data
				BlockDataOut.insert(BlockDataOut.end(), { CurrentBlock, Length });
				z += (int32_t)Length;
			}
		}
	}
}

void FChunk::ShutDown(FPhysicsSystem& PhysicsSystem)
{
	// Remove collision data from physics system
	if (!mIsEmpty)
		PhysicsSystem.RemoveCollider(mCollisionData->Object);

	mMesh->ClearBackBuffer();
}

bool FChunk::IsLoaded() const
{
	return mIsLoaded;
}

void FChunk::Render(const GLenum RenderMode)
{
	ASSERT(mIsLoaded);

	mMesh->Render(RenderMode);
}

void FChunk::SwapMeshBuffer(FPhysicsSystem& PhysicsSystem)
{
	bool WasEmpty = (mMesh->GetIndexCount() == 0);
	mMesh->SwapBuffer();
	mMesh->ClearBackBuffer();
	mIsEmpty = (mMesh->GetIndexCount() == 0);

	// Update collision info
	if (!mIsEmpty)
	{
		// Set vertex properties for collision mesh
		const int32_t IndexStride = 3 * sizeof(uint32_t);
		const int32_t VertexStride = sizeof(Vector3f);

		// Build final collision mesh
		btIndexedMesh VertexData;
		VertexData.m_triangleIndexStride = IndexStride;
		VertexData.m_numTriangles = (int)mMesh->GetIndexCount() / 3;;
		VertexData.m_numVertices = (int)mMesh->GetVertexCount();
		VertexData.m_triangleIndexBase = (const unsigned char*)mMesh->GetIndexData();
		VertexData.m_vertexBase = (const unsigned char*)mMesh->GetVertexData();
		VertexData.m_vertexStride = VertexStride;

		// Reconstruct the collision shape with updated data
		mCollisionData->Mesh.getIndexedMeshArray().clear();
		mCollisionData->Mesh.getIndexedMeshArray().push_back(VertexData);
		mCollisionData->Shape.~btBvhTriangleMeshShape();
		new (&mCollisionData->Shape) btBvhTriangleMeshShape{ &mCollisionData->Mesh, false };

		if (WasEmpty)
		{
			// Previous mesh was empty
			PhysicsSystem.AddCollider(mCollisionData->Object);
		}
	}
	else if (!WasEmpty)
	{
		// Previous mesh was not empty but this one is
		PhysicsSystem.RemoveCollider(mCollisionData->Object);
	}
}

void FChunk::RebuildMesh()
{
	GreedyMesh();
}

void FChunk::SetBlock(const Vector3i& Position, FBlock::Type BlockType)
{
	mBlocks[BlockIndex(Position)].BlockType = BlockType;
}

FBlock::Type FChunk::GetBlock(const Vector3i& Position) const
{
	return mBlocks[BlockIndex(Position)].BlockType;
}

void FChunk::DestroyBlock(const Vector3i& Position)
{
	mBlocks[BlockIndex(Position)].BlockType = FBlock::None;
}

void FChunk::GreedyMesh()
{
	// Greedy mesh algorithm by Mikola Lysenko from http://0fps.net/2012/06/30/meshing-in-a-minecraft-game/
	// Java implementation from https://github.com/roboleary/GreedyMesh/blob/master/src/mygame/Main.java

	// Vertex and index data to be sent to the mesh
	FChunkMesh::VertexRenderDataPtr RenderData{ new FChunkMesh::VertexRenderData{} };
	FChunkMesh::VertexPositionDataPtr Vertices{ new FChunkMesh::VertexPositionData{} };
	FChunkMesh::IndexDataPtr Indices{ new FChunkMesh::IndexData{} };

	// Variables to be used by the algorithm
	int32_t i, j, k, Length, Width, Height, u, v, n, Side = 0;
	int32_t x[3], q[3], du[3], dv[3];

	// This mask will contain matching voxel faces as we
	// loop through each direction in the chunk
	FBlock Mask[CHUNK_SIZE * CHUNK_SIZE];

	// Start with a for loop the will flip face direction once we iterate through
	// the chunk in one direction.
	for (bool BackFace = true, b = false; b != BackFace; BackFace = BackFace && b, b = !b)
	{

		// Iterate through each dimension of the chunk
		for (int32_t d = 0; d < 3; d++)
		{
			// Get the other 2 axes
			u = (d + 1) % 3;
			v = (d + 2) % 3;

			x[0] = 0;
			x[1] = 0;
			x[2] = 0;

			// Used to check against covering voxels
			q[0] = 0;
			q[1] = 0;
			q[2] = 0;
			q[d] = 1;

			if (d == 0)
			{ 
				Side = BackFace ? WEST : EAST; 
			}
			else if (d == 1) 
			{ 
				Side = BackFace ? BOTTOM : TOP; 
			}
			else if (d == 2) 
			{ 
				Side = BackFace ? SOUTH : NORTH; 
			}

			// Move through the dimension from front to back
			for (x[d] = -1; x[d] < CHUNK_SIZE;)
			{
				// Compute mask
				n = 0;

				for (x[v] = 0; x[v] < CHUNK_SIZE; x[v]++)
				{
					for (x[u] = 0; x[u] < CHUNK_SIZE; x[u]++)
					{
						// Check covering voxel
						FBlock Voxel1 = (x[d] >= 0) ? mBlocks[BlockIndex(x[0], x[1], x[2])] : FBlock{ FBlock::None };
						FBlock Voxel2 = (x[d] < CHUNK_SIZE - 1) ? mBlocks[BlockIndex(x[0] + q[0], x[1] + q[1], x[2] + q[2])] : FBlock{ FBlock::None };

						// If both voxels are active and the same type, mark the mask with an inactive block, if not
						// choose the appropriate voxel to mark
						if (Voxel1 == Voxel2)
							Mask[n++] = FBlock{ FBlock::None };
						else
							Mask[n++] = (BackFace) ? Voxel2 : Voxel1;
					}
				}

				x[d]++;

				// Generate the mesh for the mask
				n = 0;

				for (j = 0; j < CHUNK_SIZE; j++)
				{
					for (i = 0; i < CHUNK_SIZE;)
					{
						if (Mask[n].BlockType != FBlock::None)
						{
							// Compute the width
							for (Width = 1; i + Width < CHUNK_SIZE && (Mask[n + Width] == Mask[n]); Width++){}

							// Compute Height
							bool Done = false;

							for (Height = 1; j + Height < CHUNK_SIZE; Height++)
							{
								for (k = 0; k < Width; k++)
								{
									if (Mask[n + k + Height*CHUNK_SIZE] != Mask[n])
									{
										Done = true;
										break;
									}
								}

								if (Done) 
									break;
							}


							// Add the quad
							x[u] = i;
							x[v] = j;

							du[0] = 0;
							du[1] = 0;
							du[2] = 0;
							du[u] = Width;

							dv[0] = 0;
							dv[1] = 0;
							dv[2] = 0;
							dv[v] = Height;

							AddQuad(Vector3ui(x[0], x[1], x[2]),
								Vector3ui(x[0] + du[0], x[1] + du[1], x[2] + du[2]),
								Vector3ui(x[0] + du[0] + dv[0], x[1] + du[1] + dv[1], x[2] + du[2] + dv[2]),
								Vector3ui(x[0] + dv[0], x[1] + dv[1], x[2] + dv[2]),
								BackFace,
								Side,
								Mask[n],
								*Vertices,
								*RenderData,
								*Indices);


							// Zero the mask
							for (Length = 0; Length < Height; Length++)
							{
								for (k = 0; k < Width; k++)
								{
									Mask[n + k + Length * CHUNK_SIZE].BlockType = FBlock::None;
								}
							}

							// Increment counters
							i += Width;
							n += Width;
						}
						else
						{
							i++;
							n++;
						}
					}
				}
			}
		}
	}

	// Add data to mesh
	mMesh->AddRenderData(std::move(RenderData));
	mMesh->AddVertexPositions(std::move(Vertices));
	mMesh->AddIndices(std::move(Indices));
}

void FChunk::AddQuad(	const Vector3ui& BottomLeft,
						const Vector3ui& TopLeft,
						const Vector3ui& TopRight,
						const Vector3ui& BottomRight,
						const bool IsBackface,
						const uint32_t Side,
						const FBlock FaceInfo,
						std::vector<Vector3f>& VerticesOut,
						std::vector<FChunkMesh::RenderData>& RenderDataOut,
						std::vector<uint32_t>& IndicesOut)
{
	
	// Get the index offset by checking the size of the vertex list.
	uint32_t BaseIndex = VerticesOut.size();


	// Vertex layout w = x6_y6_z6_n3_null3_b8
	FChunkMesh::RenderData BottomLeftData;
	BottomLeftData.BlockType = FaceInfo.BlockType;
	BottomLeftData.x = (BottomLeft.x);
	BottomLeftData.y = (BottomLeft.y);
	BottomLeftData.z = (BottomLeft.z);
	BottomLeftData.NormalIndex = (Side);

	FChunkMesh::RenderData BottomRightData;
	BottomRightData.BlockType = FaceInfo.BlockType;
	BottomRightData.x = (BottomRight.x);
	BottomRightData.y = (BottomRight.y);
	BottomRightData.z = (BottomRight.z);
	BottomRightData.NormalIndex = (Side);

	FChunkMesh::RenderData TopLeftData;
	TopLeftData.BlockType = FaceInfo.BlockType;
	TopLeftData.x = (TopLeft.x);
	TopLeftData.y = (TopLeft.y);
	TopLeftData.z = (TopLeft.z);
	TopLeftData.NormalIndex = (Side);

	FChunkMesh::RenderData TopRightData;
	TopRightData.BlockType = FaceInfo.BlockType;
	TopRightData.x = (TopRight.x);
	TopRightData.y = (TopRight.y);
	TopRightData.z = (TopRight.z);
	TopRightData.NormalIndex = (Side);

	RenderDataOut.insert(RenderDataOut.end(), { BottomLeftData, BottomRightData, TopRightData, TopLeftData });

	VerticesOut.insert(VerticesOut.end(), { Vector3f{ BottomLeft },
											Vector3f{ BottomRight },
											Vector3f{ TopRight },
											Vector3f{ TopLeft } });
	


	// Add adjusted indices.
	if (IsBackface)
	{
		IndicesOut.insert(IndicesOut.end(), {	0 + BaseIndex, 
												1 + BaseIndex, 
												2 + BaseIndex, 
												2 + BaseIndex, 
												3 + BaseIndex, 
												0 + BaseIndex });

	}
	else
	{
		IndicesOut.insert(IndicesOut.end(), {	0 + BaseIndex, 
												3 + BaseIndex, 
												2 + BaseIndex, 
												0 + BaseIndex, 
												2 + BaseIndex, 
												1 + BaseIndex });
	}

}