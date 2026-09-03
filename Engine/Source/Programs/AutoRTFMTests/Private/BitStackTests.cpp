// Copyright Epic Games, Inc. All Rights Reserved.

#include "BitStack.h"
#include "Catch2Includes.h"

TEST_CASE("BitStack")
{
	using BitStackType = AutoRTFM::TBitStack<8>;

	auto Check = [&](const BitStackType& BitStack, std::vector<bool> Expected)
	{
		// Check the reported count is as expected.
		REQUIRE(BitStack.Num() == Expected.size());
		REQUIRE(BitStack.IsEmpty() == Expected.empty());

		for (size_t I = 0; I < Expected.size(); I++)
		{
			// Check the bit is as expected.
			REQUIRE(BitStack[I] == Expected[I]);
			REQUIRE(const_cast<const BitStackType&>(BitStack)[I] == Expected[I]);
		}
	};

	SECTION("Push, Pop")
	{
		BitStackType BitStack;
		Check(BitStack, {});

		BitStack.Push(true);
		Check(BitStack, {true});

		BitStack.Push(false);
		Check(BitStack, {true, false});

		BitStack.Push(false);
		Check(BitStack, {true, false, false});

		REQUIRE(BitStack.Pop() == false);
		Check(BitStack, {true, false});

		BitStack.Push(true);
		Check(BitStack, {true, false, true});

		BitStack.Push(true);
		Check(BitStack, {true, false, true, true});

		BitStack.Push(true); // Spill inline -> heap
		Check(BitStack, {true, false, true, true, true});

		REQUIRE(BitStack.Pop() == true);
		Check(BitStack, {true, false, true, true});
	}

	SECTION("Index operator")
	{
		BitStackType BitStack;
		BitStack.Push(true);
		BitStack.Push(true);
		BitStack.Push(false);
		BitStack.Push(true);
		BitStack.Push(true);
		Check(BitStack, {true, true, false, true, true});
		
		BitStack[0] = false;
		Check(BitStack, {false, true, false, true, true});

		BitStack[1] = false;
		Check(BitStack, {false, false, false, true, true});

		BitStack[2] = true;
		Check(BitStack, {false, false, true, true, true});

		BitStack[3] = BitStack[0];
		Check(BitStack, {false, false, true, false, true});

		BitStack[0] = BitStack[4];
		Check(BitStack, {true, false, true, false, true});
	}

	SECTION("Clear / Reset")
	{
		BitStackType BitStack;
		BitStack.Push(true);
		BitStack.Push(false);
		BitStack.Push(false);
		BitStack.Push(true);
		BitStack.Push(false);
		SECTION("Clear")
		{
			BitStack.Clear();
			Check(BitStack, {});
			BitStack.Push(true);
			BitStack.Push(false);
			Check(BitStack, {true, false});
		}
		SECTION("Reset")
		{
			BitStack.Reset();
			Check(BitStack, {});
			BitStack.Push(true);
			BitStack.Push(false);
			Check(BitStack, {true, false});
		}
	}

	SECTION("Copy Construct")
	{
		SECTION("Inline")
		{
			BitStackType Source;
			Source.Push(true);
			Source.Push(false);
			Source.Push(true);
			BitStackType Target(Source);
			Check(Source, {true, false, true});
			Check(Target, {true, false, true});
		}

		SECTION("Heap")
		{
			BitStackType Source;
			Source.Push(true);
			Source.Push(false);
			Source.Push(false);
			Source.Push(true);
			Source.Push(true);
			BitStackType Target(Source);
			Check(Source, {true, false, false, true, true});
			Check(Target, {true, false, false, true, true});
		}
	}

	SECTION("Move Construct")
	{
		SECTION("Inline")
		{
			BitStackType Source;
			Source.Push(true);
			Source.Push(true);
			Source.Push(false);
			BitStackType Target(std::move(Source));
			Check(Source, {});
			Check(Target, {true, true, false});
		}

		SECTION("Heap")
		{
			BitStackType Source;
			Source.Push(false);
			Source.Push(true);
			Source.Push(false);
			Source.Push(false);
			Source.Push(true);
			BitStackType Target(std::move(Source));
			Check(Source, {});
			Check(Target, {false, true, false, false, true});
		}
	}

	SECTION("Copy Assign")
	{
		BitStackType Source;
		BitStackType Target;
		auto CheckCopy = [&]()
		{
			SECTION("Source Empty")
			{
				Target = Source;
				Check(Source, {});
				Check(Target, {});
			}
			SECTION("Source Inline")
			{
				Source.Push(true);
				Source.Push(false);
				Source.Push(true);
				Target = Source;
				Check(Source, {true, false, true});
				Check(Target, {true, false, true});
			}
			SECTION("Source Heap")
			{
				Source.Push(false);
				Source.Push(false);
				Source.Push(true);
				Source.Push(false);
				Source.Push(true);
				Target = Source;
				Check(Source, {false, false, true, false, true});
				Check(Target, {false, false, true, false, true});
			}
		};

		SECTION("Target Empty")
		{
			CheckCopy();
		}
		SECTION("Target Inline")
		{
			Target.Push(true);
			Target.Push(true);
			Target.Push(false);
			CheckCopy();
		}
		SECTION("Target Heap")
		{
			Target.Push(false);
			Target.Push(true);
			Target.Push(false);
			Target.Push(true);
			Target.Push(false);
			CheckCopy();
		}
	}

	SECTION("Copy Assign Self")
	{
		BitStackType BitStack;
		BitStackType& Alias = BitStack;
		SECTION("Empty")
		{
			BitStack = Alias;
			Check(BitStack, {});
		}
		SECTION("Inline")
		{
			BitStack.Push(false);
			BitStack.Push(false);
			BitStack.Push(true);
			BitStack = Alias;
			Check(BitStack, {false, false, true});
		}
		SECTION("Heap")
		{
			BitStack.Push(true);
			BitStack.Push(true);
			BitStack.Push(false);
			BitStack.Push(false);
			BitStack.Push(true);
			BitStack = Alias;
			Check(BitStack, {true, true, false, false, true});
		}
	}

	SECTION("Move Assign")
	{
		BitStackType Source;
		BitStackType Target;
		auto CheckMove = [&]
		{
			SECTION("Source Empty")
			{
				Target = std::move(Source);
				Check(Source, {});
				Check(Target, {});
			}
			SECTION("Source Inline")
			{
				Source.Push(true);
				Source.Push(true);
				Source.Push(false);
				Target = std::move(Source);
				Check(Source, {});
				Check(Target, {true, true, false});
			}
			SECTION("Source Heap")
			{
				Source.Push(true);
				Source.Push(true);
				Source.Push(false);
				Source.Push(true);
				Source.Push(true);
				Target = std::move(Source);
				Check(Source, {});
				Check(Target, {true, true, false, true, true});
			}
		};

		SECTION("Target Empty")
		{
			CheckMove();
		}
		SECTION("Target Inline")
		{
			Target.Push(true);
			Target.Push(false);
			Target.Push(true);
			CheckMove();
		}
		SECTION("Target Heap")
		{
			Target.Push(true);
			Target.Push(false);
			Target.Push(false);
			Target.Push(false);
			Target.Push(true);
			CheckMove();
		}
	}

	SECTION("Move Assign Self")
	{
		BitStackType BitStack;
		BitStackType& Alias = BitStack;
		SECTION("Empty")
		{
			BitStack = std::move(Alias);
			Check(BitStack, {});
		}
		SECTION("Inline")
		{
			BitStack.Push(false);
			BitStack.Push(true);
			BitStack.Push(true);
			BitStack = std::move(Alias);
			Check(BitStack, {false, true, true});
		}
		SECTION("Heap")
		{
			BitStack.Push(true);
			BitStack.Push(false);
			BitStack.Push(true);
			BitStack.Push(true);
			BitStack.Push(false);
			BitStack = std::move(Alias);
			Check(BitStack, {true, false, true, true, false});
		}
	}

	SECTION("Soak")
	{
		BitStackType BitStack;
		std::vector<bool> Expected;
		for (int I = 0; I < 10000; I++)
		{
			int Mod100 = (I * 15485863) % 100;
			switch (Mod100)
			{
				case 0:
				{
					BitStack.Clear();
					Expected.clear();
					break;
				}
				case 1:
				{	
					BitStack.Reset();
					Expected.clear();
					break;
				}
				case 2:
				{	
					BitStackType Copy(BitStack);
					BitStack = Copy;
					break;
				}
				case 3:
				{	
					BitStackType Move(std::move(BitStack));
					BitStack = std::move(Move);
					break;
				}
				default:
				{
					if (Mod100 > 40 || Expected.empty())
					{
						BitStack.Push((I & 1) == 0);
						Expected.push_back((I & 1) == 0);
						Check(BitStack, Expected);
					}
					else
					{
						REQUIRE(BitStack.Pop() == Expected.back());
						Expected.pop_back();
						Check(BitStack, Expected);
					}
					break;
				}
			}
		}
	}
}
