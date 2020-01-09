#include "Hooks.h"
#include "xbyak/xbyak.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64_common/Relocation.h"
#include "skse64/GameStreams.h"

__declspec(dllexport) double g_silent_voice_duration_seconds = 0.0;
__declspec(dllexport) int g_is_obscript_say_say_to = false;

namespace hookedAddresses
{
	// E8 ? ? ? ? 48 8B F8 EB 02 33 FF 48 85 FF
	RelocAddr<uintptr_t>	kCachedResponseData_Ctor(ADDR_PAIR(0x000000014056D570, 0x0000000140573B70));
	uintptr_t				kCachedResponseData_Ctor_Hook = kCachedResponseData_Ctor + 0xEC;
	uintptr_t				kCachedResponseData_Ctor_Ret = kCachedResponseData_Ctor + 0xF1;

	// E8 ? ? ? ? EB 42 45 84 ED (VR Build - E8 ? ? ? ? EB 42 45 84 FF)
	RelocAddr<uintptr_t>	kUIUtils_QueueDialogSubtitles(ADDR_PAIR(0x00000001408D5B90, 0x00000001409037F0));
	uintptr_t				kUIUtils_QueueDialogSubtitles_Hook = kUIUtils_QueueDialogSubtitles + 0x4B;
	uintptr_t				kUIUtils_QueueDialogSubtitles_Show = kUIUtils_QueueDialogSubtitles + 0x58;
	uintptr_t				kUIUtils_QueueDialogSubtitles_Exit = kUIUtils_QueueDialogSubtitles + 0x11C;

	// E8 ? ? ? ? 84 C0 75 42 48 8B 2D ? ? ? ?
	RelocAddr<uintptr_t>	kASCM_DisplayQueuedNPCChatterData(ADDR_PAIR(0x00000001408CD260, 0x00000001408FA330));
	uintptr_t				kASCM_DisplayQueuedNPCChatterData_DialogSubs_Hook = kASCM_DisplayQueuedNPCChatterData + 0x1DE;
	uintptr_t				kASCM_DisplayQueuedNPCChatterData_DialogSubs_Show = kASCM_DisplayQueuedNPCChatterData + 0x1E7;
	uintptr_t				kASCM_DisplayQueuedNPCChatterData_DialogSubs_Exit = kASCM_DisplayQueuedNPCChatterData + 0x212;

	uintptr_t				kASCM_DisplayQueuedNPCChatterData_GeneralSubs_Hook = kASCM_DisplayQueuedNPCChatterData + 0xA0;
	uintptr_t				kASCM_DisplayQueuedNPCChatterData_GeneralSubs_Show = kASCM_DisplayQueuedNPCChatterData + 0xAD;
	uintptr_t				kASCM_DisplayQueuedNPCChatterData_GeneralSubs_Exit = kASCM_DisplayQueuedNPCChatterData + 0x1DE;

	// E8 ? ? ? ? F3 0F 10 35 ? ? ? ? 48 8D 4E 28
	RelocAddr<uintptr_t>	kASCM_QueueNPCChatterData(ADDR_PAIR(0x00000001408CCBB0, 0x00000001408F9C60));
	uintptr_t				kASCM_QueueNPCChatterData_Hook = kASCM_QueueNPCChatterData + 0x88;
	uintptr_t				kASCM_QueueNPCChatterData_Show = kASCM_QueueNPCChatterData + 0x95;
	uintptr_t				kASCM_QueueNPCChatterData_Exit = kASCM_QueueNPCChatterData + 0xD4;
}

#pragma push
#pragma pack (2)
struct XwmaHeader {
	char RIFF[4];         // "RIFF"
	unsigned long chunkSize;
	char XWMA[4];         // "XWMA"
	char subchunk1Id[4];  // "fmt "
	unsigned long subchunk1Size;   // size of fmt chunk
	unsigned short format;
	unsigned short numChannels;    // Needed for length calc.
	unsigned long samplesPerSec;   // Needed for length calc.
	unsigned long bytesPerSec;
	unsigned short blockAlign;
	unsigned short bitsPerSample;  // Needed for length calc.
	unsigned short extSize;
	char subchunk2Id[4];  // "dpds"
	// HIDDEN COMPILER PACKING OF 2 BYTES WILL HAPPEN HERE
	//unsigned long subchunk2Size;   // Needed for length calc. Length of dpds chunk.
//	unsigned long* subchunk2Data;  // Needed for length calc. Dpds data
};
STATIC_ASSERT(sizeof(XwmaHeader) == 42);
#pragma pop

struct FUZE {
	char name[4]; //FUZE
	UInt32 pad;
	UInt32 lipLen;
};


void SneakAtackVoicePath(CachedResponseData* Data, char* VoicePathBuffer)
{
	// overwritten code
	CALL_MEMBER_FN(&Data->voiceFilePath, Set)(VoicePathBuffer);

	if (strlen(VoicePathBuffer) < 17)
		return;

	std::string FUZPath(VoicePathBuffer), WAVPath(VoicePathBuffer), XWMPath(VoicePathBuffer);
	WAVPath.erase(0, 5);

	FUZPath.erase(0, 5);
	FUZPath.erase(FUZPath.length() - 3, 3);
	FUZPath.append("fuz");

	XWMPath.erase(0, 5);
	XWMPath.erase(XWMPath.length() - 3, 3);
	XWMPath.append("xwm");

	BSIStream* WAVStream = BSIStream::CreateInstance(WAVPath.c_str());
	BSIStream* FUZStream = BSIStream::CreateInstance(FUZPath.c_str());
	BSIStream* XWMStream = BSIStream::CreateInstance(XWMPath.c_str());

	if (g_is_obscript_say_say_to == true)
	{
#ifndef NDEBUG
		_MESSAGE("Loading Asset '%s'", FUZPath.c_str());
#endif
		BSResourceNiBinaryStream fileStream(FUZPath.c_str());
		if (fileStream.IsValid()) {
			_MESSAGE("Reading Asset from %s...", FUZPath.c_str());
			// Check if file is empty, if not check if the BOM is UTF-16
			if (FUZStream->valid) {
				FUZE fuz_header;
				UInt32	ret = fileStream.Read(&fuz_header, sizeof(FUZE));
#ifndef NDEBUG
				_MESSAGE("Reading Asset from %s...", fuz_header.name);
#endif
				fileStream.Seek(fuz_header.lipLen);
				XwmaHeader xwma_header;
				ret = fileStream.Read(&xwma_header, sizeof(XwmaHeader));

				UInt32 subchunk2Size;
				ret = fileStream.Read(&subchunk2Size, sizeof(subchunk2Size));
				UInt32 dpds_table_size = subchunk2Size / 4;
				fileStream.Seek((dpds_table_size - 2) * 4);
				UInt32 totalBytes;
				fileStream.Read(&totalBytes, sizeof(uint32_t));
				float numSamples = float(totalBytes) / float(xwma_header.numChannels * (xwma_header.bitsPerSample / 8));
				g_silent_voice_duration_seconds = numSamples / xwma_header.samplesPerSec;
			}

		}
	} else {

#if 0
		_MESSAGE("Expected: %s", VoicePathBuffer);
		gLog.Indent();
		_MESSAGE("WAV Stream [%s] Validity = %d", WAVPath.c_str(), WAVStream->valid);
		_MESSAGE("FUZ Stream [%s] Validity = %d", FUZPath.c_str(), FUZStream->valid);
		_MESSAGE("XWM Stream [%s] Validity = %d", XWMPath.c_str(), XWMStream->valid);
		gLog.Outdent();
#endif

#ifndef NDEBUG
		if (!(WAVStream->valid == 0 && FUZStream->valid == 0 && XWMStream->valid == 0))
#else
		if (WAVStream->valid == 0 && FUZStream->valid == 0 && XWMStream->valid == 0)
#endif
		{
			static const int kWordsPerSecond = kWordsPerSecondSilence.GetData().i;
			static const int kMaxSeconds = 10;

			int SecondsOfSilence = 2;
			char ShimAssetFilePath[0x104] = { 0 };
			std::string ResponseText(Data->responseText.Get());

			if (ResponseText.length() > 4 && strncmp(ResponseText.c_str(), "<ID=", 4))
			{
				SME::StringHelpers::Tokenizer TextParser(ResponseText.c_str(), " ");
				int WordCount = 0;

				while (TextParser.NextToken(ResponseText) != -1)
					WordCount++;

				SecondsOfSilence = WordCount / ((kWordsPerSecond > 0) ? kWordsPerSecond : 2) + 1;

				if (SecondsOfSilence <= 0)
					SecondsOfSilence = 2;
				else if (SecondsOfSilence > kMaxSeconds)
					SecondsOfSilence = kMaxSeconds;

				// calculate the response text's hash and stash it for later lookups
				SubtitleHasher::Instance.Add(Data->responseText.Get());
			}

			if (ResponseText.length() > 1 || (ResponseText.length() == 1 && ResponseText[0] == ' ' && kSkipEmptyResponses.GetData().i == 0))
			{
				FORMAT_STR(ShimAssetFilePath, "Data\\Sound\\Voice\\Fuz Ro Doh\\Stock_%d.xwm", SecondsOfSilence);
				CALL_MEMBER_FN(&Data->voiceFilePath, Set)(ShimAssetFilePath);
#ifndef NDEBUG
				_MESSAGE("Missing Asset - Switching to '%s'", ShimAssetFilePath);
#endif
			}
		}
	}

	WAVStream->Dtor();
	FUZStream->Dtor();
	XWMStream->Dtor();
}

bool ShouldForceSubs(NPCChatterData* ChatterData, UInt32 ForceRegardless, const char* Subtitle)
{
	bool Result = false;

	if (Subtitle && SubtitleHasher::Instance.HasMatch(Subtitle))		// force if the subtitle is for a voiceless response
	{
#ifndef NDEBUG
		_MESSAGE("Found a match for %s - Forcing subs", Subtitle);
#endif

		Result = true;
	}
	else if (ForceRegardless || (ChatterData && ChatterData->forceSubtitles))
		Result = true;
	else
	{
		TESTopicInfo* CurrentTopicInfo = nullptr;
		PlayerDialogData* Selection = nullptr;

		if (override::MenuTopicManager::GetSingleton()->selectedResponseNode)
			Selection = override::MenuTopicManager::GetSingleton()->selectedResponseNode->Head.Data;
		else
			Selection = override::MenuTopicManager::GetSingleton()->lastSelectedResponse;

		if (Selection)
			CurrentTopicInfo = Selection->parentTopicInfo;
		else if (override::MenuTopicManager::GetSingleton()->rootTopicInfo)
			CurrentTopicInfo = override::MenuTopicManager::GetSingleton()->rootTopicInfo;
		else
			CurrentTopicInfo = override::MenuTopicManager::GetSingleton()->unk14;

		if (CurrentTopicInfo)
		{
			if ((CurrentTopicInfo->dialogFlags >> 9) & 1)		// force subs flag's set
				Result = true;
		}
	}

	return Result;
}

#define PUSH_VOLATILE		push(rcx); push(rdx); push(r8);
#define POP_VOLATILE		pop(r8); pop(rdx); pop(rcx);

bool InstallHooks()
{
	if (!g_branchTrampoline.Create(1024 * 64))
	{
		_ERROR("Couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
		return false;
	}

	if (!g_localTrampoline.Create(1024 * 64, nullptr))
	{
		_ERROR("Couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
		return false;
	}

	{
		struct HotswapReponseAssetPath_Code : Xbyak::CodeGenerator
		{
			HotswapReponseAssetPath_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label RetnLabel;

				push(rcx);
				push(rdx);		// asset path
				mov(rcx, rbx);	// cached response
				mov(rax, (uintptr_t)SneakAtackVoicePath);
				call(rax);
				pop(rdx);
				pop(rcx);
				jmp(ptr[rip + RetnLabel]);

			L(RetnLabel);
				dq(hookedAddresses::kCachedResponseData_Ctor_Ret);
			}
		};

		void* CodeBuf = g_localTrampoline.StartAlloc();
		HotswapReponseAssetPath_Code Code(CodeBuf);
		g_localTrampoline.EndAlloc(Code.getCurr());

		g_branchTrampoline.Write5Branch(hookedAddresses::kCachedResponseData_Ctor_Hook, uintptr_t(Code.getCode()));
	}

	{
		struct UIUtilsQueueDialogSubs_Code : Xbyak::CodeGenerator
		{
			UIUtilsQueueDialogSubs_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label ShowLabel;

				mov(rax, (uintptr_t)CanShowDialogSubtitles);
				PUSH_VOLATILE;
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				PUSH_VOLATILE;
				xor(rcx, rcx);
				xor(rdx, rdx);
#ifdef VR_BUILD
				mov(r8, rbp);	// subtitle
#else
				mov(r8, r14);	// subtitle
#endif
				mov(rax, (uintptr_t)ShouldForceSubs);
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				jmp(ptr[rip]);
				dq(hookedAddresses::kUIUtils_QueueDialogSubtitles_Exit);

			L(ShowLabel);
				jmp(ptr[rip]);
				dq(hookedAddresses::kUIUtils_QueueDialogSubtitles_Show);
			}
		};

		void* CodeBuf = g_localTrampoline.StartAlloc();
		UIUtilsQueueDialogSubs_Code Code(CodeBuf);
		g_localTrampoline.EndAlloc(Code.getCurr());

		g_branchTrampoline.Write5Branch(hookedAddresses::kUIUtils_QueueDialogSubtitles_Hook, uintptr_t(Code.getCode()));
	}

	{
		struct ASCMDisplayQueuedNPCChatterData_DialogSubs_Code : Xbyak::CodeGenerator
		{
			ASCMDisplayQueuedNPCChatterData_DialogSubs_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label ShowLabel;

				mov(rax, (uintptr_t)CanShowDialogSubtitles);
				PUSH_VOLATILE;
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				PUSH_VOLATILE;
				mov(rcx, r14);
				xor(rdx, rdx);
				mov(r8, ptr[r14 + 0x8]);	// subtitle
				mov(rax, (uintptr_t)ShouldForceSubs);
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				jmp(ptr[rip]);
				dq(hookedAddresses::kASCM_DisplayQueuedNPCChatterData_DialogSubs_Exit);

			L(ShowLabel);
				jmp(ptr[rip]);
				dq(hookedAddresses::kASCM_DisplayQueuedNPCChatterData_DialogSubs_Show);
			}
		};

		void* CodeBuf = g_localTrampoline.StartAlloc();
		ASCMDisplayQueuedNPCChatterData_DialogSubs_Code Code(CodeBuf);
		g_localTrampoline.EndAlloc(Code.getCurr());

		g_branchTrampoline.Write5Branch(hookedAddresses::kASCM_DisplayQueuedNPCChatterData_DialogSubs_Hook, uintptr_t(Code.getCode()));
	}

	{
		struct ASCMDisplayQueuedNPCChatterData_GeneralSubs_Code : Xbyak::CodeGenerator
		{
			ASCMDisplayQueuedNPCChatterData_GeneralSubs_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label ShowLabel;

				mov(rax, (uintptr_t)CanShowGeneralSubtitles);
				PUSH_VOLATILE;
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				PUSH_VOLATILE;
				mov(rcx, r14);
				xor (rdx, rdx);
				mov(r8, ptr[r14 + 0x8]);	// subtitle
				mov(rax, (uintptr_t)ShouldForceSubs);
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				jmp(ptr[rip]);
				dq(hookedAddresses::kASCM_DisplayQueuedNPCChatterData_GeneralSubs_Exit);

			L(ShowLabel);
				jmp(ptr[rip]);
				dq(hookedAddresses::kASCM_DisplayQueuedNPCChatterData_GeneralSubs_Show);
			}
		};

		void* CodeBuf = g_localTrampoline.StartAlloc();
		ASCMDisplayQueuedNPCChatterData_GeneralSubs_Code Code(CodeBuf);
		g_localTrampoline.EndAlloc(Code.getCurr());

		g_branchTrampoline.Write5Branch(hookedAddresses::kASCM_DisplayQueuedNPCChatterData_GeneralSubs_Hook, uintptr_t(Code.getCode()));
	}

	{
		struct ASCMQueueNPCChatterData_Code : Xbyak::CodeGenerator
		{
			ASCMQueueNPCChatterData_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
			{
				Xbyak::Label ShowLabel;

				mov(rax, (uintptr_t)CanShowDialogSubtitles);
				PUSH_VOLATILE;
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				PUSH_VOLATILE;
				xor(rcx, rcx);
				mov(rdx, r12d);
				mov(r8, rbp);	// subtitle
				mov(rax, (uintptr_t)ShouldForceSubs);
				call(rax);
				POP_VOLATILE;
				test(al, al);
				jnz(ShowLabel);

				jmp(ptr[rip]);
				dq(hookedAddresses::kASCM_QueueNPCChatterData_Exit);

			L(ShowLabel);
				jmp(ptr[rip]);
				dq(hookedAddresses::kASCM_QueueNPCChatterData_Show);
			}
		};

		void* CodeBuf = g_localTrampoline.StartAlloc();
		ASCMQueueNPCChatterData_Code Code(CodeBuf);
		g_localTrampoline.EndAlloc(Code.getCurr());

		g_branchTrampoline.Write5Branch(hookedAddresses::kASCM_QueueNPCChatterData_Hook, uintptr_t(Code.getCode()));
	}

	return true;
}
