#include "MachoObjectFile.h"

using namespace Jitter;

CMachoObjectFile::CMachoObjectFile(CPU_ARCH cpuArch)
: CObjectFile(cpuArch)
{

}

CMachoObjectFile::~CMachoObjectFile()
{

}

void CMachoObjectFile::Write(Framework::CStream& stream)
{
	auto internalSymbolInfos = InternalSymbolInfoArray(m_internalSymbols.size());
	auto externalSymbolInfos = ExternalSymbolInfoArray(m_externalSymbols.size());

	StringTable stringTable;
	stringTable.push_back(0x00);
	FillStringTable(stringTable, m_internalSymbols, internalSymbolInfos);
	FillStringTable(stringTable, m_externalSymbols, externalSymbolInfos);

	auto textSection = BuildSection(m_internalSymbols, internalSymbolInfos, INTERNAL_SYMBOL_LOCATION_TEXT);
	auto dataSection = BuildSection(m_internalSymbols, internalSymbolInfos, INTERNAL_SYMBOL_LOCATION_DATA);
	
	//Make sure section size is aligned
	textSection.data.resize((textSection.data.size() + 0x03) & ~0x03);
	dataSection.data.resize((dataSection.data.size() + 0x01) & ~0x01);

	auto symbols = BuildSymbols(m_internalSymbols, internalSymbolInfos, m_externalSymbols, externalSymbolInfos,
		0, textSection.data.size());

	auto textSectionRelocations = BuildRelocations(textSection, internalSymbolInfos, externalSymbolInfos);
	auto dataSectionRelocations = BuildRelocations(dataSection, internalSymbolInfos, externalSymbolInfos);

	uint32 sectionCount = 2;
	SectionArray sections;

	uint32 sizeofCommands = sizeof(Macho::SEGMENT_COMMAND) + (sizeof(Macho::SECTION) * sectionCount) + 
		sizeof(Macho::SYMTAB_COMMAND);
	uint32 headerSize = sizeof(Macho::MACH_HEADER) + sizeofCommands;
	uint32 segmentDataSize = textSection.data.size() + dataSection.data.size();
	uint32 segmentAndRelocDataSize = segmentDataSize + (textSectionRelocations.size() + dataSectionRelocations.size()) * sizeof(Macho::RELOCATION_INFO);

	{
		Macho::SECTION section;
		memset(&section, 0, sizeof(Macho::SECTION));
		strncpy(section.sectionName, "__text", 0x10);
		strncpy(section.segmentName, "__TEXT", 0x10);
		
		section.sectionName[0x0F] = 0;
		section.segmentName[0x0F] = 0;

		section.address				= 0;
		section.size				= textSection.data.size();
		section.offset				= headerSize;
		section.align				= 0x02;
		section.relocationOffset	= headerSize + segmentDataSize;
		section.relocationCount		= textSectionRelocations.size();
		section.flags				= Macho::S_ATTR_SOME_INSTRUCTIONS | Macho::S_ATTR_PURE_INSTRUCTIONS;
		section.reserved1			= 0;
		section.reserved2			= 0;

		sections.push_back(section);
	}

	{
		Macho::SECTION section;
		memset(&section, 0, sizeof(Macho::SECTION));
		strncpy(section.sectionName, "__data", 0x10);
		strncpy(section.segmentName, "__DATA", 0x10);
		
		section.sectionName[0x0F] = 0;
		section.segmentName[0x0F] = 0;

		section.address				= textSection.data.size();
		section.size				= dataSection.data.size();
		section.offset				= headerSize + textSection.data.size();
		section.align				= 0x01;
		section.relocationOffset	= headerSize + segmentDataSize + textSectionRelocations.size() * sizeof(Macho::RELOCATION_INFO);
		section.relocationCount		= dataSectionRelocations.size();
		section.flags				= 0;
		section.reserved1			= 0;
		section.reserved2			= 0;

		sections.push_back(section);
	}

	Macho::SEGMENT_COMMAND segmentCommand;
	memset(&segmentCommand, 0, sizeof(Macho::SEGMENT_COMMAND));
	segmentCommand.cmd				= Macho::LC_SEGMENT;
	segmentCommand.cmdSize			= sizeof(Macho::SEGMENT_COMMAND) + (sizeof(Macho::SECTION) * sections.size());
	segmentCommand.vmAddress		= 0;
	segmentCommand.vmSize			= segmentDataSize;
	segmentCommand.fileOffset		= headerSize;
	segmentCommand.fileSize			= segmentDataSize;
	segmentCommand.maxProtection	= 0x07;
	segmentCommand.initProtection	= 0x07;
	segmentCommand.sectionCount		= sections.size();
	segmentCommand.flags			= 0;

	Macho::SYMTAB_COMMAND symtabCommand;
	memset(&symtabCommand, 0, sizeof(Macho::SYMTAB_COMMAND));
	symtabCommand.cmd				= Macho::LC_SYMTAB;
	symtabCommand.cmdSize			= sizeof(Macho::SYMTAB_COMMAND);
	symtabCommand.stringsSize		= stringTable.size();
	symtabCommand.stringsOffset		= headerSize + segmentAndRelocDataSize + (sizeof(Macho::NLIST) * symbols.size());
	symtabCommand.symbolCount		= symbols.size();
	symtabCommand.symbolsOffset		= headerSize + segmentAndRelocDataSize;

	Macho::MACH_HEADER header = {};
	header.magic			= Macho::MH_MAGIC;
	switch(m_cpuArch)
	{
	case CPU_ARCH_X86:
		header.cpuType			= Macho::CPU_TYPE_I386;
		header.cpuSubType		= Macho::CPU_SUBTYPE_I386_ALL;
		break;
	case CPU_ARCH_ARM:
		header.cpuType			= Macho::CPU_TYPE_ARM;
		header.cpuSubType		= Macho::CPU_SUBTYPE_ARM_V7;
		break;
	default:
		throw std::runtime_error("MachoObjectFile: Unsupported CPU architecture.");
		break;
	}
	header.fileType			= Macho::MH_OBJECT;
	header.commandCount		= 2;		//SEGMENT_COMMMAND + SYMTAB_COMMAND
	header.sizeofCommands	= sizeofCommands;
	header.flags			= Macho::MH_NOMULTIDEFS;

	stream.Write(&header, sizeof(Macho::MACH_HEADER));
	stream.Write(&segmentCommand, sizeof(Macho::SEGMENT_COMMAND));
	for(const auto& section : sections)
	{
		stream.Write(&section, sizeof(Macho::SECTION));
	}
	stream.Write(&symtabCommand, sizeof(Macho::SYMTAB_COMMAND));
	stream.Write(textSection.data.data(), textSection.data.size());
	stream.Write(dataSection.data.data(), dataSection.data.size());
	stream.Write(textSectionRelocations.data(), textSectionRelocations.size() * sizeof(Macho::RELOCATION_INFO));
	stream.Write(dataSectionRelocations.data(), dataSectionRelocations.size() * sizeof(Macho::RELOCATION_INFO));
	stream.Write(symbols.data(), sizeof(Macho::NLIST) * symbols.size());
	stream.Write(stringTable.data(), stringTable.size());
}

void CMachoObjectFile::FillStringTable(StringTable& stringTable, const InternalSymbolArray& internalSymbols, InternalSymbolInfoArray& internalSymbolInfos)
{
	uint32 stringTableSizeIncrement = 0;
	for(const auto& internalSymbol : internalSymbols)
	{
		stringTableSizeIncrement += internalSymbol.name.length() + 1;
	}
	stringTable.reserve(stringTable.size() + stringTableSizeIncrement);
	for(uint32 i = 0; i < internalSymbols.size(); i++)
	{
		const auto& internalSymbol = internalSymbols[i];
		auto& internalSymbolInfo = internalSymbolInfos[i];
		internalSymbolInfo.nameOffset = stringTable.size();
		stringTable.insert(std::end(stringTable), std::begin(internalSymbol.name), std::end(internalSymbol.name));
		stringTable.push_back(0);
	}
}

void CMachoObjectFile::FillStringTable(StringTable& stringTable, const ExternalSymbolArray& externalSymbols, ExternalSymbolInfoArray& externalSymbolInfos)
{
	uint32 stringTableSizeIncrement = 0;
	for(const auto& externalSymbol : externalSymbols)
	{
		stringTableSizeIncrement += externalSymbol.name.length() + 1;
	}
	stringTable.reserve(stringTable.size() + stringTableSizeIncrement);
	for(uint32 i = 0; i < externalSymbols.size(); i++)
	{
		const auto& externalSymbol = externalSymbols[i];
		auto& externalSymbolInfo = externalSymbolInfos[i];
		externalSymbolInfo.nameOffset = stringTable.size();
		stringTable.insert(std::end(stringTable), std::begin(externalSymbol.name), std::end(externalSymbol.name));
		stringTable.push_back(0);
	}
}

CMachoObjectFile::SECTION CMachoObjectFile::BuildSection(const InternalSymbolArray& internalSymbols, InternalSymbolInfoArray& internalSymbolInfos, INTERNAL_SYMBOL_LOCATION location)
{
	SECTION section;
	auto& sectionData(section.data);
	uint32 sectionSize = 0;
	for(const auto& internalSymbol : internalSymbols)
	{
		sectionSize += internalSymbol.data.size();
	}
	sectionData.reserve(sectionSize);
	for(uint32 i = 0; i < internalSymbols.size(); i++)
	{
		const auto& internalSymbol = internalSymbols[i];
		if(internalSymbol.location != location) continue;

		auto& internalSymbolInfo = internalSymbolInfos[i];
		internalSymbolInfo.dataOffset = sectionData.size();
		for(const auto& symbolReference : internalSymbol.symbolReferences)
		{
			SYMBOL_REFERENCE newReference;
			newReference.offset			= symbolReference.offset + internalSymbolInfo.dataOffset;
			newReference.symbolIndex	= symbolReference.symbolIndex;
			newReference.type			= symbolReference.type;
			section.symbolReferences.push_back(newReference);
		}
		sectionData.insert(std::end(sectionData), std::begin(internalSymbol.data), std::end(internalSymbol.data));
	}
	return section;
}

CMachoObjectFile::SymbolArray CMachoObjectFile::BuildSymbols(const InternalSymbolArray& internalSymbols, InternalSymbolInfoArray& internalSymbolInfos, 
														   const ExternalSymbolArray& externalSymbols, ExternalSymbolInfoArray& externalSymbolInfos,
														   uint32 textSectionVa, uint32 dataSectionVa)
{
	SymbolArray symbols;
	symbols.reserve(internalSymbols.size() + externalSymbols.size());

	//Internal symbols
	for(uint32 i = 0; i < internalSymbols.size(); i++)
	{
		const auto& internalSymbol = internalSymbols[i];
		auto& internalSymbolInfo = internalSymbolInfos[i];
		internalSymbolInfo.symbolIndex = symbols.size();

		Macho::NLIST symbol = {};
		symbol.stringTableIndex		 = internalSymbolInfo.nameOffset;
		symbol.type					 = 0x1F;		// N_SECT | N_PEXT | N_EXT
		symbol.value				 = internalSymbolInfo.dataOffset;
		symbol.section				 = (internalSymbol.location == CObjectFile::INTERNAL_SYMBOL_LOCATION_TEXT) ? 1 : 2;		//.text or .data section
		symbol.value				+= (internalSymbol.location == CObjectFile::INTERNAL_SYMBOL_LOCATION_TEXT) ? textSectionVa : dataSectionVa;
		symbol.desc					 = 0;
		symbols.push_back(symbol);
	}

	//External symbols
	for(uint32 i = 0; i < externalSymbols.size(); i++)
	{
		const auto& externalSymbol = externalSymbols[i];
		auto& externalSymbolInfo = externalSymbolInfos[i];
		externalSymbolInfo.symbolIndex = symbols.size();

		Macho::NLIST symbol = {};
		symbol.stringTableIndex		= externalSymbolInfo.nameOffset;
		symbol.value				= 0;
		symbol.type					= 0x01;		// N_UNDF | N_EXT
		symbol.section				= 0;
		symbol.desc					= 0;
		symbols.push_back(symbol);
	}

	return symbols;
}

CMachoObjectFile::RelocationArray CMachoObjectFile::BuildRelocations(SECTION& section, const InternalSymbolInfoArray& internalSymbolInfos, const ExternalSymbolInfoArray& externalSymbolInfos)
{
	RelocationArray relocations;
	relocations.resize(section.symbolReferences.size());

	for(uint32 i = 0; i < section.symbolReferences.size(); i++)
	{
		const auto& symbolReference = section.symbolReferences[i];
		uint32 symbolIndex = (symbolReference.type == SYMBOL_TYPE_INTERNAL) ? 
			internalSymbolInfos[symbolReference.symbolIndex].symbolIndex :
			externalSymbolInfos[symbolReference.symbolIndex].symbolIndex;

		auto& relocation = relocations[i];
		relocation.type				= 0;			//GENERIC_RELOC_VANILLA
		relocation.symbolIndex		= symbolIndex;
		relocation.address			= symbolReference.offset;
		relocation.isExtern			= 1;
		relocation.pcRel			= 0;
		relocation.length			= 2;			//4 bytes

		*reinterpret_cast<uint32*>(section.data.data() + symbolReference.offset) = 0;
	}

	return relocations;
}