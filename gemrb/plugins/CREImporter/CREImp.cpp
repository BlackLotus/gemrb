/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * $Id$
 *
 */

#include "../../includes/win32def.h"
#include "CREImp.h"
#include "../Core/Interface.h"
#include "../Core/EffectMgr.h"
#include "../Core/GameScript.h"
#include "../../includes/ie_stats.h"
#include <cassert>

#define DEFAULT_MOVEMENTRATE 9 //this is 9 in ToB

#define MAXCOLOR 12
typedef unsigned char ColorSet[MAXCOLOR];

static int RandColor=-1;
static int RandRows=-1;
static ColorSet* randcolors=NULL;
static int MagicBit = core->HasFeature(GF_MAGICBIT);

void ReleaseMemoryCRE()
{
	if (randcolors) {
		delete [] randcolors;
		randcolors = NULL;
	}
	RandColor = -1;
}

CREImp::CREImp(void)
{
	str = NULL;
	autoFree = false;
	TotSCEFF = 0xff;
	CREVersion = 0xff;
}

CREImp::~CREImp(void)
{
	if (str && autoFree) {
		delete( str );
	}
}

bool CREImp::Open(DataStream* stream, bool aF)
{
	if (str && this->autoFree) {
		delete( str );
	}
	str = stream;
	autoFree = aF;
	if (stream == NULL) {
		return false;
	}
	char Signature[8];
	str->Read( Signature, 8 );
	IsCharacter = false;
	if (strncmp( Signature, "CHR ",4) == 0) {
		IsCharacter = true;
		//skips chr signature, reads cre signature
		if (!SeekCreHeader(Signature)) {
			return false;
		}
	} else {
		CREOffset = 0;
	}
	if (strncmp( Signature, "CRE V1.0", 8 ) == 0) {
		CREVersion = IE_CRE_V1_0;
		return true;
	}
	if (strncmp( Signature, "CRE V1.2", 8 ) == 0) {
		CREVersion = IE_CRE_V1_2;
		return true;
	}
	if (strncmp( Signature, "CRE V2.2", 8 ) == 0) {
		CREVersion = IE_CRE_V2_2;
		return true;
	}
	if (strncmp( Signature, "CRE V9.0", 8 ) == 0) {
		CREVersion = IE_CRE_V9_0;
		return true;
	}
	if (strncmp( Signature, "CRE V0.0", 8 ) == 0) {
		CREVersion = IE_CRE_GEMRB;
		return true;
	}

	printMessage( "CREImporter"," ",LIGHT_RED);
	printf("Not a CRE File or File Version not supported: %8.8s\n", Signature );
	return false;
}

void CREImp::SetupSlotCounts()
{
	switch (CREVersion) {
		case IE_CRE_V1_2: //pst
			QWPCount=4;
			QSPCount=3;
			QITCount=5;
			break;
		case IE_CRE_GEMRB: //own
			QWPCount=8;
			QSPCount=9;
			QITCount=5;
			break;
		case IE_CRE_V2_2: //iwd2
			QWPCount=8;
			QSPCount=9;
			QITCount=3;
			break;
		default: //others
			QWPCount=4;
			QSPCount=3;
			QITCount=3;
			break;
	}
}

void CREImp::WriteChrHeader(DataStream *stream, Actor *act)
{
	char Signature[8];
	ieVariable name;
	ieDword tmpDword, CRESize;
	ieWord tmpWord;
	ieByte tmpByte;

	CRESize = GetStoredFileSize (act);
	switch (CREVersion) {
		case IE_CRE_V9_0: //iwd/HoW
			memcpy(Signature, "CHR V1.0",8);
			tmpDword = 0x64; //headersize
			TotSCEFF = 1;
			break;
		case IE_CRE_V1_0: //bg1
			memcpy(Signature, "CHR V1.0",8);
			tmpDword = 0x64; //headersize
			TotSCEFF = 0;
			break;
		case IE_CRE_V1_1: //bg2 (fake)
			memcpy(Signature, "CHR V2.0",8);
			tmpDword = 0x64; //headersize
			TotSCEFF = 1;
			break;
		case IE_CRE_V1_2: //pst
			memcpy(Signature, "CHR V1.2",8);
			tmpDword = 0x68; //headersize
			TotSCEFF = 0;
			break;
		case IE_CRE_V2_2:  //iwd2
			memcpy(Signature, "CHR V2.2",8);
			tmpDword = 0x21c; //headersize
			TotSCEFF = 1;
			break;
		case IE_CRE_GEMRB: //own format
			memcpy(Signature, "CHR V0.0",8);
			tmpDword = 0x1dc; //headersize (iwd2-9x8+8)
			TotSCEFF = 1;
			break;
		default:
			printMessage("CREImporter","Unknown CHR version!\n",LIGHT_RED);
			return;
	}
	stream->Write( Signature, 8);
	memset( Signature,0,sizeof(Signature));
	memset( name,0,sizeof(name));
	strncpy( name, act->GetName(0), sizeof(name) );
	stream->Write( name, 32);

	stream->WriteDword( &tmpDword); //cre offset (chr header size)
	stream->WriteDword( &CRESize);  //cre size

	SetupSlotCounts();
	int i;
	for (i=0;i<QWPCount;i++) {
		tmpWord = act->PCStats->QuickWeaponSlots[i];
		stream->WriteWord (&tmpWord);
	}
	for (i=0;i<QWPCount;i++) {
		tmpWord = act->PCStats->QuickWeaponHeaders[i];
		stream->WriteWord (&tmpWord);
	}
	for (i=0;i<QSPCount;i++) {
		stream->WriteResRef (act->PCStats->QuickSpells[i]);
	}
	if (QSPCount==9) {
		stream->Write( act->PCStats->QuickSpellClass,9);
		tmpByte = 0;
		stream->Write( &tmpByte, 1);
	}
	for (i=0;i<QITCount;i++) {
		tmpWord = act->PCStats->QuickItemSlots[i];
		stream->WriteWord (&tmpWord);
	}
	for (i=0;i<QITCount;i++) {
		tmpWord = act->PCStats->QuickItemHeaders[i];
		stream->WriteWord (&tmpWord);
	}
	switch (CREVersion) {
	case IE_CRE_V2_2:
		//IWD2 quick innates are saved elsewhere, redundantly
		for (i=0;i<QSPCount;i++) {
			stream->WriteResRef (act->PCStats->QuickSpells[i]);
		}
		//fallthrough
	case IE_CRE_GEMRB:
		for (i=0;i<18;i++) {
			stream->WriteDword( &tmpDword);
		}
		for (i=0;i<QSPCount;i++) {
			tmpDword = act->PCStats->QSlots[i];
			stream->WriteDword( &tmpDword);
		}
		for (i=0;i<13;i++) {
			stream->WriteWord (&tmpWord);
		}
		stream->Write( act->PCStats->SoundFolder, 32);
		stream->Write( act->PCStats->SoundSet, 8);
		for (i=0;i<32;i++) {
			stream->WriteDword( &tmpDword);
		}
		break;
	default:
		break;
	}
}

void CREImp::ReadChrHeader(Actor *act)
{
	ieVariable name;
	char Signature[8];
	ieDword offset, size;
	ieDword tmpDword;
	ieWord tmpWord;
	ieByte tmpByte;

	act->CreateStats();
	str->Rewind();
	str->Read (Signature, 8);
	str->Read (name, 32);
	name[32]=0;
	tmpDword = *(ieDword *) name;
	if (tmpDword != 0 && tmpDword !=1) {
		act->SetText( name, 0 ); //setting longname
	}
	str->ReadDword( &offset);
	str->ReadDword( &size);
	SetupSlotCounts();
	int i;
	for (i=0;i<QWPCount;i++) {
		str->ReadWord (&tmpWord);
		act->PCStats->QuickWeaponSlots[i]=tmpWord;
	}
	for (i=0;i<QWPCount;i++) {
		str->ReadWord (&tmpWord);
		act->PCStats->QuickWeaponHeaders[i]=tmpWord;
	}
	for (i=0;i<QSPCount;i++) {
		str->ReadResRef (act->PCStats->QuickSpells[i]);
	}
	if (QSPCount==9) {
		str->Read (act->PCStats->QuickSpellClass,9);
		str->Read (&tmpByte, 1);
	}
	for (i=0;i<QITCount;i++) {
		str->ReadWord (&tmpWord);
		act->PCStats->QuickItemSlots[i]=tmpWord;
	}
	for (i=0;i<QITCount;i++) {
		str->ReadWord (&tmpWord);
		act->PCStats->QuickItemHeaders[i]=tmpWord;
	}
	//here comes the version specific read
}

bool CREImp::SeekCreHeader(char *Signature)
{
	str->Seek(32, GEM_CURRENT_POS);
	str->ReadDword( &CREOffset );
	str->Seek(CREOffset, GEM_STREAM_START);
	str->Read( Signature, 8);
	return true;
}

CREMemorizedSpell* CREImp::GetMemorizedSpell()
{
	CREMemorizedSpell* spl = new CREMemorizedSpell();

	str->ReadResRef( spl->SpellResRef );
	str->ReadDword( &spl->Flags );

	return spl;
}

CREKnownSpell* CREImp::GetKnownSpell()
{
	CREKnownSpell* spl = new CREKnownSpell();

	str->ReadResRef( spl->SpellResRef );
	str->ReadWord( &spl->Level );
	str->ReadWord( &spl->Type );

	return spl;
}

void CREImp::ReadScript(Actor *act, int ScriptLevel)
{
	ieResRef aScript;
	str->ReadResRef( aScript );
	act->SetScript( aScript, ScriptLevel);
}

CRESpellMemorization* CREImp::GetSpellMemorization()
{
	CRESpellMemorization* spl = new CRESpellMemorization();

	str->ReadWord( &spl->Level );
	str->ReadWord( &spl->Number );
	str->ReadWord( &spl->Number2 );
	str->ReadWord( &spl->Type );
	str->ReadDword( &MemorizedIndex );
	str->ReadDword( &MemorizedCount );

	return spl;
}

void CREImp::SetupColor(ieDword &stat)
{
	if (RandColor==-1) {
		RandColor=0;
		RandRows=0;
		int table = core->LoadTable( "randcolr" );
		TableMgr *rndcol = core->GetTable( table );
		if (rndcol) {
			RandColor = rndcol->GetColumnCount();
			RandRows = rndcol->GetRowCount();
			if (RandRows>MAXCOLOR) RandRows=MAXCOLOR;
		}
		if (RandRows>1 && RandColor>0) {
			randcolors = new ColorSet[RandColor];
			int cols = RandColor;
			while(cols--)
			{
				for (int i=0;i<RandRows;i++) {
					randcolors[cols][i]=atoi( rndcol->QueryField( i, cols ) );
				}
				randcolors[cols][0]-=200;
			}
		}
		else {
			RandColor=0;
		}
		core->DelTable( table );
	}

	if (stat<200) return;
	if (RandColor>0) {
		stat-=200;
		//assuming an ordered list, so looking in the middle first
		int i;
		for (i=(int) stat;i>=0;i--) {
			if (randcolors[i][0]==stat) {
				stat=randcolors[i][ rand()%RandRows + 1];
				return;
			}
		}
		for (i=(int) stat+1;i<RandColor;i++) {
			if (randcolors[i][0]==stat) {
				stat=randcolors[i][ rand()%RandRows + 1];
				return;
			}
		}
	}
}

Actor* CREImp::GetActor()
{
	if (!str)
		return NULL;
	Actor* act = new Actor();
	if (!act)
		return NULL;
	act->InParty = 0;
	str->ReadDword( &act->LongStrRef );
	//Beetle name in IWD needs the allow zero flag
	char* poi = core->GetString( act->LongStrRef, IE_STR_ALLOW_ZERO );
	act->SetText( poi, 1 ); //setting longname
	free( poi );
	str->ReadDword( &act->ShortStrRef );
	poi = core->GetString( act->ShortStrRef );
	act->SetText( poi, 2 ); //setting shortname (for tooltips)
	free( poi );
	act->BaseStats[IE_VISUALRANGE] = 30; //this is just a hack
	act->BaseStats[IE_DIALOGRANGE] = 15; //this is just a hack
	str->ReadDword( &act->BaseStats[IE_MC_FLAGS] );
	str->ReadDword( &act->BaseStats[IE_XPVALUE] );
	str->ReadDword( &act->BaseStats[IE_XP] );
	str->ReadDword( &act->BaseStats[IE_GOLD] );
	str->ReadDword( &act->BaseStats[IE_STATE_ID] );
	ieWord tmp;
	str->ReadWord( &tmp );
	act->BaseStats[IE_HITPOINTS]=tmp;
	str->ReadWord( &tmp );
	act->BaseStats[IE_MAXHITPOINTS]=tmp;
	str->ReadDword( &act->BaseStats[IE_ANIMATION_ID] );//animID is a dword
	ieByte tmp2[7];
	str->Read( tmp2, 7);
	for (int i=0;i<7;i++) {
		ieDword t = tmp2[i];
		// apply RANDCOLR.2DA transformation
		SetupColor(t);
		t |= t << 8;
		t |= t << 16;
		act->BaseStats[IE_COLORS+i]=t;
	}

	str->Read( &TotSCEFF, 1 );
	if (CREVersion==IE_CRE_V1_0 && TotSCEFF) {
		CREVersion = IE_CRE_V1_1;
	}
	// saving in original version requires the original version
	// otherwise it is set to 0 at construction time
	if (core->SaveAsOriginal) {
		act->version = CREVersion;
	}
	str->ReadResRef( act->SmallPortrait );
	if (act->SmallPortrait[0]==0) {
		memcpy(act->SmallPortrait, "NONE\0\0\0\0", 8);
	}
	str->ReadResRef( act->LargePortrait );
	if (act->LargePortrait[0]==0) {
		memcpy(act->LargePortrait, "NONE\0\0\0\0", 8);
	}

	unsigned int Inventory_Size;

	switch(CREVersion) {
		case IE_CRE_GEMRB:
			Inventory_Size = GetActorGemRB(act);
			break;
		case IE_CRE_V1_2:
			Inventory_Size=46;
			GetActorPST(act);
			break;
		case IE_CRE_V1_1: //bg2 (fake version)
		case IE_CRE_V1_0: //bg1 too
			Inventory_Size=38;
			GetActorBG(act);
			break;
		case IE_CRE_V2_2:
			Inventory_Size=50;
			GetActorIWD2(act);
			break;
		case IE_CRE_V9_0:
			Inventory_Size=38;
			GetActorIWD1(act);
			break;
		default:
			Inventory_Size=0;
			printMessage("CREImp","Unknown creature signature.\n", YELLOW);
	}

	ieResRef Dialog;
	str->ReadResRef(Dialog);
	//Hacking NONE to no error
	if (strnicmp(Dialog,"NONE",8) == 0) {
		Dialog[0]=0;
	}
	act->SetDialog(Dialog);

	// Read saved effects
	if (core->IsAvailable(IE_EFF_CLASS_ID) ) {
		ReadEffects( act );
	} else {
		printf("Effect importer is unavailable!\n");
	}
	// Reading inventory, spellbook, etc
	ReadInventory( act, Inventory_Size );

	act->SetBase(IE_MOVEMENTRATE, DEFAULT_MOVEMENTRATE);
	//this is required so the actor has animation already
	act->SetAnimationID( ( ieWord ) act->BaseStats[IE_ANIMATION_ID] );
	// Setting up derived stats
	if (act->BaseStats[IE_STATE_ID] & STATE_DEAD) {
		act->SetStance( IE_ANI_TWITCH );
		act->Deactivate();
	} else {
		act->SetStance( IE_ANI_AWAKE );
	}
	if (IsCharacter) {
		ReadChrHeader(act);
	}

	if (CREVersion==IE_CRE_V1_2) {
		ieDword hp = act->BaseStats[IE_HITPOINTS] - GetHpAdjustment(act);
		act->BaseStats[IE_HITPOINTS]=hp;
	}
	act->inventory.CalculateWeight();
	if (act->BaseStats[IE_CLASSLEVELSUM]) {
		act->CreateDerivedStatsIWD2();
	} else {
		act->CreateDerivedStatsBG();
	}
	return act;
}

void CREImp::GetActorPST(Actor *act)
{
	int i;
	ieByte tmpByte;
	ieWord tmpWord;

	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_REPUTATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HIDEINSHADOWS]=tmpByte;
	str->ReadWord( &tmpWord );
	//skipping a word
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ARMORCLASS]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACCRUSHINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACMISSILEMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACPIERCINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACSLASHINGMOD]=(ieWordSigned) tmpWord;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TOHIT]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	tmpByte = tmpByte * 2;
	if (tmpByte>10) tmpByte-=11;
	act->BaseStats[IE_NUMBEROFATTACKS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSDEATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSWANDS]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSPOLY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSBREATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSSPELL]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTELECTRICITY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTACID]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGIC]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTSLASHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCRUSHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTPIERCING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMISSILE]=(ieByteSigned) tmpByte;
	//this is used for unused prof points count
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FREESLOTS]=tmpByte; //using another field than usually
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SETTRAPS]=tmpByte; //this is unused in pst
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LORE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LOCKPICKING]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STEALTH]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_PICKPOCKET]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FATIGUE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INTOXICATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LUCK]=tmpByte;
	for (i=0;i<21;i++) {
		str->Read( &tmpByte, 1 );
		act->BaseStats[IE_PROFICIENCYBASTARDSWORD+i]=tmpByte;
	}
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRACKING]=tmpByte;
	str->Seek( 32, GEM_CURRENT_POS );
	for (i=0;i<100;i++) {
		str->ReadDword( &act->StrRefs[i] );
	}
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL2]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL3]=tmpByte;
	//this is rumoured to be IE_SEX, but we use the gender field for this
	str->Read( &tmpByte, 1 );
	//skipping a byte
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STREXTRA]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INT]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_WIS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_DEX]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CON]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CHR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALEBREAK]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HATEDRACE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALERECOVERYTIME]=tmpByte;
	str->Read( &tmpByte, 1 );
	//skipping a byte
	str->ReadDword( &act->BaseStats[IE_KIT] );
	ReadScript(act, SCR_OVERRIDE);
	ReadScript(act, SCR_CLASS);
	ReadScript(act, SCR_RACE);
	ReadScript(act, SCR_GENERAL);
	ReadScript(act, SCR_DEFAULT);

	str->Seek( 36, GEM_CURRENT_POS );
	//the overlays are not fully decoded yet
	//they are a kind of effect block (like our vvclist)
	str->ReadDword( &OverlayOffset );
	str->ReadDword( &OverlayMemorySize );
	str->ReadDword( &act->BaseStats[IE_XP_MAGE] ); // Exp for secondary class
	str->ReadDword( &act->BaseStats[IE_XP_THIEF] ); // Exp for tertiary class
	for (i = 0; i<10; i++) {
		str->ReadWord( &tmpWord );
		act->BaseStats[IE_INTERNAL_0+i]=tmpWord;
	}
	str->Seek( 4, GEM_CURRENT_POS );
	ieVariable KillVar; //use this as needed
	str->Read(KillVar,32);
	KillVar[32]=0;
	str->Seek( 3, GEM_CURRENT_POS ); // dialog radius, feet circle size???

	ieByte ColorsCount;

	str->Read( &ColorsCount, 1 );

	str->ReadWord( &act->AppearanceFlags1 );
	str->ReadWord( &act->AppearanceFlags2 );

	for (i = 0; i < 7; i++) {
		str->ReadWord( &tmpWord );
		act->BaseStats[IE_COLORS+i] = tmpWord;
	}
	act->BaseStats[IE_COLORCOUNT] = ColorsCount; //hack

	str->Seek(31, GEM_CURRENT_POS);
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SPECIES]=tmpByte; // offset: 0x311
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TEAM]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FACTION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_EA]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_GENERAL]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RACE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CLASS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SPECIFIC]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SEX]=tmpByte;
	str->Seek( 5, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_ALIGNMENT]=tmpByte;
	str->Seek( 4, GEM_CURRENT_POS );
	ieVariable scriptname;
	str->Read( scriptname, 32);
	scriptname[32]=0;
	act->SetScriptName(scriptname);
	strnspccpy(act->KillVar, KillVar, 32);

	str->ReadDword( &KnownSpellsOffset );
	str->ReadDword( &KnownSpellsCount );
	str->ReadDword( &SpellMemorizationOffset );
	str->ReadDword( &SpellMemorizationCount );
	str->ReadDword( &MemorizedSpellsOffset );
	str->ReadDword( &MemorizedSpellsCount );

	str->ReadDword( &ItemSlotsOffset );
	str->ReadDword( &ItemsOffset );
	str->ReadDword( &ItemsCount );
	str->ReadDword( &EffectsOffset );
	str->ReadDword( &EffectsCount ); //also variables

	//str->ReadResRef( act->Dialog );
}

void CREImp::ReadInventory(Actor *act, unsigned int Inventory_Size)
{
	CREItem** items;
	unsigned int i,j,k;

	str->Seek( ItemsOffset+CREOffset, GEM_STREAM_START );
	items = (CREItem **) calloc (ItemsCount, sizeof(CREItem *) );

	for (i = 0; i < ItemsCount; i++) {
		items[i] = core->ReadItem(str); //could be NULL item
	}
	act->inventory.SetSlotCount(Inventory_Size+1);

	str->Seek( ItemSlotsOffset+CREOffset, GEM_STREAM_START );
	act->SetupFist();

	for (i = 1; i <= Inventory_Size; i++) {
		int Slot = core->QuerySlot( i );
		ieWord index;
		str->Read( &index, 2 );

		if (index != 0xFFFF) {
			if (index>=ItemsCount) {
				printMessage("CREImp"," ",LIGHT_RED);
				printf("Invalid item index (%d) in creature!\n", index);
				continue;
			}
			CREItem *item = items[index];
			if (item && core->Exists(item->ItemResRef, IE_ITM_CLASS_ID)) {
				act->inventory.SetSlotItem( item, Slot );
				/* this stuff will be done in Actor::SetMap
				 * after the actor gets an area (and game obj)
				int slottype = core->QuerySlotEffects( Slot );
				//weapons will be equipped differently, see SetEquippedSlot later
				//i think missiles equipping effects are always
				//in effect, if not, then add SLOT_EFFECT_MISSILE
				if (slottype != SLOT_EFFECT_NONE && slottype != SLOT_EFFECT_MELEE) {
					act->inventory.EquipItem( Slot, true );
				}
				*/
				items[index] = NULL;
				continue;
			}
			printMessage("CREImp"," ",LIGHT_RED);
			printf("Duplicate or (no-drop) item (%d) in creature!\n", index);
		}
	}

	i = ItemsCount;
	while(i--) {
		if ( items[i]) {
			printMessage("CREImp"," ",LIGHT_RED);
			printf("Dangling item in creature: %s!\n", items[i]->ItemResRef);
			delete items[i];
		}
	}
	free (items);

	//this dword contains the equipping info (which slot is selected)
	// 0,1,2,3 - weapon slots
	// 1000 - fist
	// -24,-23,-22,-21 - quiver
	//the equipping effects are delayed until the actor gets an area
	//ieDword Equipped;
	str->ReadDword( &act->Equipped );
	//act->inventory.SetEquippedSlot( (short)Equipped, true);

	// Reading spellbook
	CREKnownSpell **known_spells=(CREKnownSpell **) calloc(KnownSpellsCount, sizeof(CREKnownSpell *) );
	CREMemorizedSpell **memorized_spells=(CREMemorizedSpell **) calloc(MemorizedSpellsCount, sizeof(CREKnownSpell *) );

	str->Seek( KnownSpellsOffset+CREOffset, GEM_STREAM_START );
	for (i = 0; i < KnownSpellsCount; i++) {
		known_spells[i]=GetKnownSpell();
	}

	str->Seek( MemorizedSpellsOffset+CREOffset, GEM_STREAM_START );
	for (i = 0; i < MemorizedSpellsCount; i++) {
		memorized_spells[i]=GetMemorizedSpell();
	}

	str->Seek( SpellMemorizationOffset+CREOffset, GEM_STREAM_START );
	for (i = 0; i < SpellMemorizationCount; i++) {
		CRESpellMemorization* sm = GetSpellMemorization();

		j=KnownSpellsCount;
		while(j--) {
			CREKnownSpell* spl = known_spells[j];
			if (!spl) {
				continue;
			}
			if ((spl->Type == sm->Type) && (spl->Level == sm->Level)) {
				sm->known_spells.push_back( spl );
				known_spells[j] = NULL;
				continue;
			}
		}
		for (j = 0; j < MemorizedCount; j++) {
			k = MemorizedIndex+j;
			if (memorized_spells[k]) {
				sm->memorized_spells.push_back( memorized_spells[k]);
				memorized_spells[k] = NULL;
				continue;
			}
			printf("[CREImp]: Duplicate memorized spell (%d) in creature!\n", k);
		}
		act->spellbook.AddSpellMemorization( sm );
	}

	i=KnownSpellsCount;
	while(i--) {
		if (known_spells[i]) {
			printMessage("CREImp"," ", YELLOW);
			printf("Dangling spell in creature: %s!\n", known_spells[i]->SpellResRef);
			delete known_spells[i];
		}
	}
	free(known_spells);

	i=MemorizedSpellsCount;
	while(i--) {
		if (memorized_spells[i]) {
			printMessage("CREImp"," ", YELLOW);
			printf("Dangling spell in creature: %s!\n", memorized_spells[i]->SpellResRef);
			delete memorized_spells[i];
		}
	}
	free(memorized_spells);
}

void CREImp::ReadEffects(Actor *act)
{
	unsigned int i;

	str->Seek( EffectsOffset+CREOffset, GEM_STREAM_START );

	for (i = 0; i < EffectsCount; i++) {
		Effect fx;
		GetEffect( &fx );
		// NOTE: AddEffect() allocates a new effect
		act->fxqueue.AddEffect( &fx ); // FIXME: don't reroll dice, time, etc!!
	}
}

void CREImp::GetEffect(Effect *fx)
{
	EffectMgr* eM = ( EffectMgr* ) core->GetInterface( IE_EFF_CLASS_ID );

	eM->Open( str, false );
	if (TotSCEFF) {
		eM->GetEffectV20( fx );
	} else {
		eM->GetEffectV1( fx );
	}
	core->FreeInterface( eM );

}

ieDword CREImp::GetActorGemRB(Actor *act)
{
	ieByte tmpByte;
	ieWord tmpWord;

	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_REPUTATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HIDEINSHADOWS]=tmpByte;
	//skipping a word( useful for something)
	str->ReadWord( &tmpWord );
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ARMORCLASS]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACCRUSHINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACMISSILEMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACPIERCINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACSLASHINGMOD]=(ieWordSigned) tmpWord;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TOHIT]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_NUMBEROFATTACKS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSDEATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSWANDS]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSPOLY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSBREATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSSPELL]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTELECTRICITY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTACID]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGIC]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTSLASHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCRUSHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTPIERCING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMISSILE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_DETECTILLUSIONS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SETTRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LORE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LOCKPICKING]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STEALTH]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_PICKPOCKET]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FATIGUE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INTOXICATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LUCK]=tmpByte;
	//these could be used to save iwd2 skills
	return 0;
}

void CREImp::GetActorBG(Actor *act)
{
	int i;
	ieByte tmpByte;
	ieWord tmpWord;

	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_REPUTATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HIDEINSHADOWS]=tmpByte;
	str->ReadWord( &tmpWord );
	//skipping a word
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ARMORCLASS]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACCRUSHINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACMISSILEMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACPIERCINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACSLASHINGMOD]=(ieWordSigned) tmpWord;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TOHIT]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	tmpWord = tmpByte * 2;
	if (tmpWord>10) tmpWord-=11;
	act->BaseStats[IE_NUMBEROFATTACKS]=(ieByte) tmpWord;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSDEATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSWANDS]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSPOLY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSBREATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSSPELL]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTELECTRICITY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTACID]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGIC]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTSLASHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCRUSHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTPIERCING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMISSILE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_DETECTILLUSIONS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SETTRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LORE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LOCKPICKING]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STEALTH]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_PICKPOCKET]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FATIGUE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INTOXICATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LUCK]=tmpByte;
	for (i=0;i<21;i++) {
		str->Read( &tmpByte, 1 );
		act->BaseStats[IE_PROFICIENCYBASTARDSWORD+i]=tmpByte;
	}

	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRACKING]=tmpByte;
	str->Seek( 32, GEM_CURRENT_POS );
	for (i=0;i<100;i++) {
		str->ReadDword( &act->StrRefs[i] );
	}
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL2]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL3]=tmpByte;
	//this is rumoured to be IE_SEX, but we use the gender field for this
	str->Read( &tmpByte, 1);
	//skipping a byte
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_STR]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_STREXTRA]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_INT]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_WIS]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_DEX]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CON]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CHR]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_MORALE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_MORALEBREAK]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_HATEDRACE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_MORALERECOVERYTIME]=tmpByte;
	str->Read( &tmpByte, 1);
	//skipping a byte
	str->ReadDword( &act->BaseStats[IE_KIT] );
	ReadScript(act, SCR_OVERRIDE);
	ReadScript(act, SCR_CLASS);
	ReadScript(act, SCR_RACE);
	ReadScript(act, SCR_GENERAL);
	ReadScript(act, SCR_DEFAULT);

	str->Read( &tmpByte, 1);
	act->BaseStats[IE_EA]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_GENERAL]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_RACE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CLASS]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SPECIFIC]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SEX]=tmpByte;
	str->Seek( 5, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_ALIGNMENT]=tmpByte;
	str->Seek( 4, GEM_CURRENT_POS );
	ieVariable scriptname;
	str->Read( scriptname, 32);
	scriptname[32]=0;
	act->SetScriptName(scriptname);
	act->KillVar[0]=0;

	str->ReadDword( &KnownSpellsOffset );
	str->ReadDword( &KnownSpellsCount );
	str->ReadDword( &SpellMemorizationOffset );
	str->ReadDword( &SpellMemorizationCount );
	str->ReadDword( &MemorizedSpellsOffset );
	str->ReadDword( &MemorizedSpellsCount );

	str->ReadDword( &ItemSlotsOffset );
	str->ReadDword( &ItemsOffset );
	str->ReadDword( &ItemsCount );
	str->ReadDword( &EffectsOffset );
	str->ReadDword( &EffectsCount );

	//str->ReadResRef( act->Dialog );
}

void CREImp::GetActorIWD2(Actor *act)
{
	int i;
	ieByte tmpByte;
	ieWord tmpWord;

	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_REPUTATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HIDEINSHADOWS]=tmpByte;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ARMORCLASS]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACCRUSHINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACMISSILEMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACPIERCINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACSLASHINGMOD]=(ieWordSigned) tmpWord;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TOHIT]=(ieByteSigned) tmpByte;//Unknown in CRE V2.2
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_NUMBEROFATTACKS]=tmpByte;//Unknown in CRE V2.2
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSDEATH]=(ieByteSigned) tmpByte;//Fortitude Save in V2.2
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSWANDS]=(ieByteSigned) tmpByte;//Reflex Save in V2.2
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSPOLY]=(ieByteSigned) tmpByte;// will Save in V2.2
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTELECTRICITY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTACID]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGIC]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTSLASHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCRUSHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTPIERCING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMISSILE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MAGICDAMAGERESISTANCE]=(ieByteSigned) tmpByte;
	str->Seek( 4, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FATIGUE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INTOXICATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LUCK]=tmpByte;
	str->Seek( 34, GEM_CURRENT_POS ); //unknowns
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CLASSLEVELSUM]=tmpByte; //total levels
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELBARBARIAN]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELBARD]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELCLERIC]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELDRUID]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELFIGHTER]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELMONK]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELPALADIN]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELRANGER]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELTHIEF]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELSORCEROR]=tmpByte;
	str->Read( & tmpByte, 1 );
	act->BaseStats[IE_LEVELMAGE]=tmpByte;
	str->Seek( 22, GEM_CURRENT_POS ); //levels for classes
	for (i=0;i<64;i++) {
		str->ReadDword( &act->StrRefs[i] );
	}
	ReadScript( act, SCR_AREA);
	ReadScript( act, SCR_RESERVED);
	str->Seek( 4, GEM_CURRENT_POS );
	str->ReadDword( &act->BaseStats[IE_FEATS1]);
	str->ReadDword( &act->BaseStats[IE_FEATS2]);
	str->ReadDword( &act->BaseStats[IE_FEATS3]);
	str->Seek( 12, GEM_CURRENT_POS );
	//proficiencies
	for (i=0;i<26;i++) {
		str->Read( &tmpByte, 1);
		act->BaseStats[IE_PROFICIENCYBASTARDSWORD+i]=tmpByte;
	}
	//skills
	str->Seek( 38, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_ALCHEMY]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_ANIMALS]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_BLUFF]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CONCENTRATION]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_DIPLOMACY]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_TRAPS]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_HIDEINSHADOWS]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_INTIMIDATE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_LORE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_STEALTH]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_LOCKPICKING]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_PICKPOCKET]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SEARCH]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SPELLCRAFT]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_MAGICDEVICE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_TRACKING]=tmpByte;
	str->Seek( 50, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HATEDRACE]=tmpByte;
	//we got 7 more hated races
	for (i=0;i<7;i++) {
		str->Read( &tmpByte, 1 );
		act->BaseStats[IE_HATEDRACE2+i]=tmpByte;
	}
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SUBRACE]=tmpByte;
	str->ReadWord( &tmpWord );
	//skipping 2 bytes, one is SEX (could use it for sounds)
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INT]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_WIS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_DEX]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CON]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CHR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALEBREAK]=tmpByte;
	str->Read( &tmpByte, 1 );
	//HatedRace is a list of races, so this is skipped here
	//act->BaseStats[IE_HATEDRACE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALERECOVERYTIME]=tmpByte;
	str->ReadDword( &act->BaseStats[IE_KIT] );
	ReadScript(act, SCR_OVERRIDE);
	ReadScript(act, SCR_CLASS);
	ReadScript(act, SCR_RACE);
	ReadScript(act, SCR_GENERAL);
	ReadScript(act, SCR_DEFAULT);
	//new scripting flags, one on each byte
	str->Read( &tmpByte, 1); //hidden
	if (tmpByte) {
		act->BaseStats[IE_AVATARREMOVAL]=tmpByte;
	}
	str->Read( &tmpByte, 1); //set death variable
	str->Read( &tmpByte, 1); //increase kill count
	str->Read( &tmpByte, 1); //unknown
	for (i = 0; i<5; i++) {
		str->ReadWord( &tmpWord );
		act->BaseStats[IE_INTERNAL_0+i]=tmpWord;
	}
	ieVariable KillVar;
	str->Read(KillVar,32); //use these as needed
	KillVar[32]=0;
	str->Read(KillVar,32);
	KillVar[32]=0;
	str->Seek( 2, GEM_CURRENT_POS);
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_SAVEDXPOS] = tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_SAVEDYPOS] = tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_SAVEDFACE] = tmpWord;
	str->Seek( 146, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_EA]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_GENERAL]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_RACE]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CLASS]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SPECIFIC]=tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SEX]=tmpByte;
	str->Seek( 5, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_ALIGNMENT]=tmpByte;
	str->Seek( 4, GEM_CURRENT_POS );
	ieVariable scriptname;
	str->Read( scriptname, 32);
	scriptname[32]=0;
	act->SetScriptName(scriptname);
	strnspccpy(act->KillVar, KillVar, 32);

	KnownSpellsOffset = 0;
	KnownSpellsCount = 0;
	SpellMemorizationOffset = 0;
	SpellMemorizationCount = 0;
	MemorizedSpellsOffset = 0;
	MemorizedSpellsCount = 0;
	//6 bytes unknown, 600 bytes spellbook offsets
	//skipping spellbook offsets
	ieWord tmp1, tmp2, tmp3;
	str->ReadWord( &tmp1);
	str->ReadWord( &tmp2);
	str->ReadWord( &tmp3);
	ieDword ClassSpellOffsets[8*9];

	//spellbook spells
	for (i=0;i<7*9;i++) {
		str->ReadDword(ClassSpellOffsets+i);
	}
	ieDword ClassSpellCounts[8*9];
	for (i=0;i<7*9;i++) {
		str->ReadDword(ClassSpellCounts+i);
	}

	//domain spells
	for (i=7*9;i<8*9;i++) {
		str->ReadDword(ClassSpellOffsets+i);
	}
	for (i=7*9;i<8*9;i++) {
		str->ReadDword(ClassSpellCounts+i);
	}

	ieDword InnateOffset, InnateCount;
	ieDword SongOffset, SongCount;
	ieDword ShapeOffset, ShapeCount;
	str->ReadDword( &InnateOffset );
	str->ReadDword( &InnateCount );
	str->ReadDword( &SongOffset );
	str->ReadDword( &SongCount );
	str->ReadDword( &ShapeOffset );
	str->ReadDword( &ShapeCount );
	//str->Seek( 606, GEM_CURRENT_POS);

	str->ReadDword( &ItemSlotsOffset );
	str->ReadDword( &ItemsOffset );
	str->ReadDword( &ItemsCount );
	str->ReadDword( &EffectsOffset );
	str->ReadDword( &EffectsCount );

	//str->ReadResRef( act->Dialog );
}

void CREImp::GetActorIWD1(Actor *act) //9.0
{
	int i;
	ieByte tmpByte;
	ieWord tmpWord;

	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_REPUTATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HIDEINSHADOWS]=tmpByte;
	str->ReadWord( &tmpWord );
	//skipping a word
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ARMORCLASS]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACCRUSHINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACMISSILEMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACPIERCINGMOD]=(ieWordSigned) tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_ACSLASHINGMOD]=(ieWordSigned) tmpWord;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TOHIT]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	tmpByte = tmpByte * 2;
	if (tmpByte>10) tmpByte-=11;
	act->BaseStats[IE_NUMBEROFATTACKS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSDEATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSWANDS]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSPOLY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSBREATH]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SAVEVSSPELL]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTELECTRICITY]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTACID]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGIC]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICFIRE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMAGICCOLD]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTSLASHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTCRUSHING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTPIERCING]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_RESISTMISSILE]=(ieByteSigned) tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_DETECTILLUSIONS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_SETTRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LORE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LOCKPICKING]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STEALTH]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRAPS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_PICKPOCKET]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_FATIGUE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INTOXICATION]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LUCK]=tmpByte;
	for (i=0;i<21;i++) {
		str->Read( &tmpByte, 1 );
		act->BaseStats[IE_PROFICIENCYBASTARDSWORD+i]=tmpByte;
	}
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_TRACKING]=tmpByte;
	str->Seek( 32, GEM_CURRENT_POS );
	for (i=0;i<100;i++) {
		str->ReadDword( &act->StrRefs[i] );
	}
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL2]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_LEVEL3]=tmpByte;
	//this is rumoured to be IE_SEX, but we use the gender field for this
	str->Read( &tmpByte, 1 );
	//skipping a byte
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_STREXTRA]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_INT]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_WIS]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_DEX]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CON]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_CHR]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALEBREAK]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_HATEDRACE]=tmpByte;
	str->Read( &tmpByte, 1 );
	act->BaseStats[IE_MORALERECOVERYTIME]=tmpByte;
	str->Read( &tmpByte, 1 );
	//skipping a byte
	str->ReadDword( &act->BaseStats[IE_KIT] );
	ReadScript(act, SCR_OVERRIDE);
	ReadScript(act, SCR_CLASS);
	ReadScript(act, SCR_RACE);
	ReadScript(act, SCR_GENERAL);
	ReadScript(act, SCR_DEFAULT);
	//new scripting flags, one on each byte
	str->Read( &tmpByte, 1); //hidden
	if (tmpByte) {
		act->BaseStats[IE_AVATARREMOVAL]=tmpByte;
	}
	str->Read( &tmpByte, 1); //set death variable
	str->Read( &tmpByte, 1); //increase kill count
	str->Read( &tmpByte, 1); //unknown
	for (i = 0; i<5; i++) {
		str->ReadWord( &tmpWord );
		act->BaseStats[IE_INTERNAL_0+i]=tmpWord;
	}
	ieVariable KillVar;
	str->Read(KillVar,32); //use these as needed
	KillVar[32]=0;
	str->Read(KillVar,32);
	KillVar[32]=0;
	str->Seek( 2, GEM_CURRENT_POS);
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_SAVEDXPOS] = tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_SAVEDYPOS] = tmpWord;
	str->ReadWord( &tmpWord );
	act->BaseStats[IE_SAVEDFACE] = tmpWord;
	str->Seek( 18, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_EA] = tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_GENERAL] = tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_RACE] = tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_CLASS] = tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SPECIFIC] = tmpByte;
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_SEX] = tmpByte;
	str->Seek( 5, GEM_CURRENT_POS );
	str->Read( &tmpByte, 1);
	act->BaseStats[IE_ALIGNMENT]=tmpByte;
	str->Seek( 4, GEM_CURRENT_POS );
	ieVariable scriptname;
	str->Read( scriptname, 32);
	scriptname[32]=0;
	act->SetScriptName(scriptname);
	strnspccpy(act->KillVar, KillVar, 32);

	str->ReadDword( &KnownSpellsOffset );
	str->ReadDword( &KnownSpellsCount );
	str->ReadDword( &SpellMemorizationOffset );
	str->ReadDword( &SpellMemorizationCount );
	str->ReadDword( &MemorizedSpellsOffset );
	str->ReadDword( &MemorizedSpellsCount );

	str->ReadDword( &ItemSlotsOffset );
	str->ReadDword( &ItemsOffset );
	str->ReadDword( &ItemsCount );
	str->ReadDword( &EffectsOffset );
	str->ReadDword( &EffectsCount );

	//str->ReadResRef( act->Dialog );
}

int CREImp::GetStoredFileSize(Actor *actor)
{
	int headersize;
	unsigned int Inventory_Size;
	unsigned int i;

	CREVersion = actor->version;
	switch (CREVersion) {
		case IE_CRE_GEMRB:
			headersize = 0x2d4;
			//minus fist
			Inventory_Size=actor->inventory.GetSlotCount()-1;
			TotSCEFF = 1;
			break;
		case IE_CRE_V1_1://totsc/bg2/tob (still V1.0, but large effects)
		case IE_CRE_V1_0://bg1
			headersize = 0x2d4;
			Inventory_Size=38;
			//we should know it is bg1
			if (actor->version == IE_CRE_V1_1) {
				TotSCEFF = 1;
			} else {
				TotSCEFF = 0;
			}
			break;
		case IE_CRE_V1_2: //pst
			headersize = 0x378;
			Inventory_Size=46;
			TotSCEFF = 0;
			break;
		case IE_CRE_V2_2://iwd2
			headersize = 0x62e; //with offsets
			Inventory_Size=50;
			TotSCEFF = 1;
			break;
		case IE_CRE_V9_0://iwd
			headersize = 0x33c;
			Inventory_Size=38;
			TotSCEFF = 1;
			break;
		default:
			return -1;
	}
	KnownSpellsOffset = headersize;

	if (actor->version==IE_CRE_V2_2) { //iwd2
		//adding spellbook header sizes
		//class spells, domains, (shapes,songs,innates)
		headersize += 7*9*8 + 9*8 + 3*8;
	} else {//others
		//adding known spells
		KnownSpellsCount = actor->spellbook.GetTotalKnownSpellsCount();
		headersize += KnownSpellsCount * 12;
		SpellMemorizationOffset = headersize;

		//adding spell pages
		SpellMemorizationCount = actor->spellbook.GetTotalPageCount();
		headersize += SpellMemorizationCount * 16;
		MemorizedSpellsOffset = headersize;

		MemorizedSpellsCount = actor->spellbook.GetTotalMemorizedSpellsCount();
		headersize += MemorizedSpellsCount * 12;
	}
	EffectsOffset = headersize;

	//adding effects
	EffectsCount = actor->fxqueue.GetSavedEffectsCount();
	VariablesCount = actor->locals->GetCount();
	if (VariablesCount) {
		TotSCEFF=1;
	}
	if (TotSCEFF) {
		headersize += (VariablesCount + EffectsCount) * 264;
	} else {
		//if there are variables, then TotSCEFF is set
		headersize += EffectsCount * 48;
	}
	ItemsOffset = headersize;

	//counting items (calculating item storage)
	ItemsCount = 0;
	for (i=0;i<Inventory_Size;i++) {
	//for (i=0;i<core->GetInventorySize();i++) {
		unsigned int j = core->QuerySlot(i+1);
		CREItem *it = actor->inventory.GetSlotItem(j);
		if (it) {
			ItemsCount++;
		}
	}
	headersize += ItemsCount * 20;
	ItemSlotsOffset = headersize;

	//adding itemslot table size and equipped slot field
	return headersize + (Inventory_Size)*sizeof(ieWord)+sizeof(ieDword);
}

int CREImp::PutInventory(DataStream *stream, Actor *actor, unsigned int size)
{
	unsigned int i;
	ieDword tmpDword;
	ieWord ItemCount = 0;
	ieWord *indices =(ieWord *) malloc(size*sizeof(ieWord) );

	for (i=0;i<size;i++) {
		indices[i]=(ieWord) -1;
	}

	for (i=0;i<size;i++) {
		//ignore first element, getinventorysize makes space for fist
		unsigned int j = core->QuerySlot(i+1);
		CREItem *it = actor->inventory.GetSlotItem( j );
		if (!it) {
			continue;
		}
		stream->WriteResRef( it->ItemResRef);
		stream->WriteWord( &it->Expired);
		stream->WriteWord( &it->Usages[0]);
		stream->WriteWord( &it->Usages[1]);
		stream->WriteWord( &it->Usages[2]);
		tmpDword = it->Flags;
		//IWD uses this bit differently
		if (MagicBit) {
			if (it->Flags&IE_INV_ITEM_MAGICAL) {
				tmpDword|=IE_INV_ITEM_UNDROPPABLE;
			} else {
				tmpDword&=~IE_INV_ITEM_UNDROPPABLE;
			}
		}
		stream->WriteDword( &tmpDword);
		indices[i] = ItemCount++;
	}
	for (i=0;i<size;i++) {
		stream->WriteWord( indices+i);
	}
	tmpDword = actor->inventory.GetEquipped();
	stream->WriteDword( &tmpDword);
	free(indices);
	return 0;
}

//this hack is needed for PST to adjust the current hp for compatibility
int CREImp::GetHpAdjustment(Actor *actor)
{
	int val;

	if (actor->Modified[IE_CLASS]==2) {
		val = core->GetConstitutionBonus(STAT_CON_HP_WARRIOR,actor->BaseStats[IE_CON]);
	} else {
		val = core->GetConstitutionBonus(STAT_CON_HP_NORMAL,actor->BaseStats[IE_CON]);
	}
	return val * actor->GetXPLevel( false );
}

int CREImp::PutHeader(DataStream *stream, Actor *actor)
{
	char Signature[8];
	ieByte tmpByte;
	ieWord tmpWord;
	int i;
	char filling[51];

	memset(filling,0,sizeof(filling));
	memcpy( Signature, "CRE V0.0", 8);
	Signature[5]+=CREVersion/10;
	if (actor->version!=IE_CRE_V1_1) {
		Signature[7]+=CREVersion%10;
	}
	stream->Write( Signature, 8);
	stream->WriteDword( &actor->ShortStrRef);
	stream->WriteDword( &actor->LongStrRef);
	stream->WriteDword( &actor->BaseStats[IE_MC_FLAGS]);
	stream->WriteDword( &actor->BaseStats[IE_XPVALUE]);
	stream->WriteDword( &actor->BaseStats[IE_XP]);
	stream->WriteDword( &actor->BaseStats[IE_GOLD]);
	stream->WriteDword( &actor->BaseStats[IE_STATE_ID]);
	tmpWord = actor->BaseStats[IE_HITPOINTS];
	if (CREVersion==IE_CRE_V1_2) {
		tmpWord = (ieWord) (tmpWord + GetHpAdjustment(actor));
	}
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_MAXHITPOINTS];
	stream->WriteWord( &tmpWord);
	stream->WriteDword( &actor->BaseStats[IE_ANIMATION_ID]);
	for (i=0;i<7;i++) {
		Signature[i] = (char) actor->BaseStats[IE_COLORS+i];
	}
	//old effect type
	Signature[7] = TotSCEFF;
	stream->Write( Signature, 8);
	stream->WriteResRef( actor->SmallPortrait);
	stream->WriteResRef( actor->LargePortrait);
	tmpByte = actor->BaseStats[IE_REPUTATION];
	stream->Write( &tmpByte, 1 );
	tmpByte = actor->BaseStats[IE_HIDEINSHADOWS];
	stream->Write( &tmpByte, 1 );
	//from here it differs, slightly
	tmpWord = actor->BaseStats[IE_ARMORCLASS];
	stream->WriteWord( &tmpWord);
	//iwd2 doesn't store this a second time,
	//probably gemrb format shouldn't either?
	if (actor->version != IE_CRE_V2_2) {
		tmpWord = actor->BaseStats[IE_ARMORCLASS];
		stream->WriteWord( &tmpWord);
	}
	tmpWord = actor->BaseStats[IE_ACCRUSHINGMOD];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_ACMISSILEMOD];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_ACPIERCINGMOD];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_ACSLASHINGMOD];
	stream->WriteWord( &tmpWord);
	tmpByte = actor->BaseStats[IE_TOHIT];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_NUMBEROFATTACKS];
	if (actor->version == IE_CRE_V2_2) {
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEFORTITUDE];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEREFLEX];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEWILL];
		stream->Write( &tmpByte, 1);
	} else {
		if (actor->version!=IE_CRE_GEMRB) {
			if (tmpByte&1) tmpByte = tmpByte/2+6;
			else tmpByte /=2;
		}
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEVSDEATH];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEVSWANDS];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEVSPOLY];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEVSBREATH];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SAVEVSSPELL];
		stream->Write( &tmpByte, 1);
	}
	tmpByte = actor->BaseStats[IE_RESISTFIRE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTCOLD];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTELECTRICITY];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTACID];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTMAGIC];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTMAGICFIRE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTMAGICCOLD];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTSLASHING];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTCRUSHING];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTPIERCING];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RESISTMISSILE];
	stream->Write( &tmpByte, 1);
	if (actor->version == IE_CRE_V2_2) {
		tmpByte = actor->BaseStats[IE_MAGICDAMAGERESISTANCE];
		stream->Write( &tmpByte, 1);
		stream->Write( Signature, 4);
	} else {
		tmpByte = actor->BaseStats[IE_DETECTILLUSIONS];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SETTRAPS];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LORE];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LOCKPICKING];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_STEALTH];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_TRAPS];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_PICKPOCKET];
		stream->Write( &tmpByte, 1);
	}
	tmpByte = actor->BaseStats[IE_FATIGUE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_INTOXICATION];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_LUCK];
	stream->Write( &tmpByte, 1);

	if (actor->version == IE_CRE_V2_2) {
		//this is rather fuzzy
		//turnundead level, + 33 bytes of zero
		tmpByte = actor->BaseStats[IE_TURNUNDEADLEVEL];
		stream->Write(&tmpByte, 1);
		stream->Write( filling,33);
		//total levels
		tmpByte = actor->BaseStats[IE_CLASSLEVELSUM];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELBARBARIAN];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELBARD];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELCLERIC];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELDRUID];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELFIGHTER];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELMONK];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELPALADIN];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELRANGER];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELTHIEF];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELSORCEROR];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVELMAGE];
		stream->Write( &tmpByte, 1);
		//some stuffing
		stream->Write( filling, 22);
		//string references
		for (i=0;i<64;i++) {
			stream->WriteDword( &actor->StrRefs[i]);
		}
		stream->WriteResRef( actor->Scripts[SCR_AREA]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_RESERVED]->GetName() );
		//unknowns before feats
		stream->Write( filling,4);
		//feats
		stream->WriteDword( &actor->BaseStats[IE_FEATS1]);
		stream->WriteDword( &actor->BaseStats[IE_FEATS2]);
		stream->WriteDword( &actor->BaseStats[IE_FEATS3]);
		stream->Write( filling, 12);
		//proficiencies
		for (i=0;i<26;i++) {
			tmpByte = actor->BaseStats[IE_PROFICIENCYBASTARDSWORD+i];
			stream->Write( &tmpByte, 1);
		}
		stream->Write( filling, 38);
		//alchemy
		tmpByte = actor->BaseStats[IE_ALCHEMY];
		stream->Write( &tmpByte, 1);
		//animals
		tmpByte = actor->BaseStats[IE_ANIMALS];
		stream->Write( &tmpByte, 1);
		//bluff
		tmpByte = actor->BaseStats[IE_BLUFF];
		stream->Write( &tmpByte, 1);
		//concentration
		tmpByte = actor->BaseStats[IE_CONCENTRATION];
		stream->Write( &tmpByte, 1);
		//diplomacy
		tmpByte = actor->BaseStats[IE_DIPLOMACY];
		stream->Write( &tmpByte, 1);
		//disarm trap
		tmpByte = actor->BaseStats[IE_TRAPS];
		stream->Write( &tmpByte, 1);
		//hide
		tmpByte = actor->BaseStats[IE_HIDEINSHADOWS];
		stream->Write( &tmpByte, 1);
		//intimidate
		tmpByte = actor->BaseStats[IE_INTIMIDATE];
		stream->Write( &tmpByte, 1);
		//lore
		tmpByte = actor->BaseStats[IE_LORE];
		stream->Write( &tmpByte, 1);
		//move silently
		tmpByte = actor->BaseStats[IE_STEALTH];
		stream->Write( &tmpByte, 1);
		//open lock
		tmpByte = actor->BaseStats[IE_LOCKPICKING];
		stream->Write( &tmpByte, 1);
		//pickpocket
		tmpByte = actor->BaseStats[IE_PICKPOCKET];
		stream->Write( &tmpByte, 1);
		//search
		tmpByte = actor->BaseStats[IE_SEARCH];
		stream->Write( &tmpByte, 1);
		//spellcraft
		tmpByte = actor->BaseStats[IE_SPELLCRAFT];
		stream->Write( &tmpByte, 1);
		//use magic device
		tmpByte = actor->BaseStats[IE_MAGICDEVICE];
		stream->Write( &tmpByte, 1);
		//tracking
		tmpByte = actor->BaseStats[IE_TRACKING];
		stream->Write( &tmpByte, 1);
		stream->Write( filling, 50);
		tmpByte = actor->BaseStats[IE_CR];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_HATEDRACE];
		stream->Write( &tmpByte, 1);
		for (i=0;i<7;i++) {
			tmpByte = actor->BaseStats[IE_HATEDRACE2+i];
			stream->Write( &tmpByte, 1);
		}
		tmpByte = actor->BaseStats[IE_SUBRACE];
		stream->Write( &tmpByte, 1);
		stream->Write( filling, 1); //unknown
		tmpByte = actor->BaseStats[IE_SEX]; //
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_STR];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_INT];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_WIS];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_DEX];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_CON];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_CHR];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_MORALE];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_MORALEBREAK];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_MORALERECOVERYTIME];
		stream->Write( &tmpByte, 1);
		// unknown byte
		stream->Write( &filling,1);
		stream->WriteDword( &actor->BaseStats[IE_KIT] );
		stream->WriteResRef( actor->Scripts[SCR_OVERRIDE]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_CLASS]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_RACE]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_GENERAL]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_DEFAULT]->GetName() );
	} else {
		for (i=0;i<21;i++) {
			tmpByte = actor->BaseStats[IE_PROFICIENCYBASTARDSWORD+i];
			stream->Write( &tmpByte, 1);
		}
		tmpByte = actor->BaseStats[IE_TRACKING];
		stream->Write( &tmpByte, 1);
		stream->Write( filling, 32);
		for (i=0;i<100;i++) {
			stream->WriteDword( &actor->StrRefs[i]);
		}
		tmpByte = actor->BaseStats[IE_LEVEL];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVEL2];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_LEVEL3];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_SEX]; //
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_STR];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_STREXTRA];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_INT];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_WIS];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_DEX];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_CON];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_CHR];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_MORALE];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_MORALEBREAK];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_HATEDRACE];
		stream->Write( &tmpByte, 1);
		tmpByte = actor->BaseStats[IE_MORALERECOVERYTIME];
		stream->Write( &tmpByte, 1);
		// unknown byte
		stream->Write( &Signature,1);
		stream->WriteDword( &actor->BaseStats[IE_KIT] );
		stream->WriteResRef( actor->Scripts[SCR_OVERRIDE]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_CLASS]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_RACE]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_GENERAL]->GetName() );
		stream->WriteResRef( actor->Scripts[SCR_DEFAULT]->GetName() );
	}
	//now follows the fuzzy part in separate putactor... functions
	return 0;
}

int CREImp::PutActorGemRB(DataStream *stream, Actor *actor, ieDword InvSize)
{
	ieByte tmpByte;
	char filling[5];

	memset(filling,0,sizeof(filling));
	//similar in all engines
	tmpByte = actor->BaseStats[IE_EA];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_GENERAL];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RACE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_CLASS];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SPECIFIC];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SEX];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 5); //unknown bytes
	tmpByte = actor->BaseStats[IE_ALIGNMENT];
	stream->Write( &tmpByte, 1);
	stream->WriteDword( &InvSize ); //saving the inventory size to this unused part
	stream->Write( actor->GetScriptName(), 32);
	return 0;
}

int CREImp::PutActorBG(DataStream *stream, Actor *actor)
{
	ieByte tmpByte;
	char filling[5];

	memset(filling,0,sizeof(filling));
	//similar in all engines
	tmpByte = actor->BaseStats[IE_EA];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_GENERAL];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RACE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_CLASS];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SPECIFIC];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SEX];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 5); //unknown bytes
	tmpByte = actor->BaseStats[IE_ALIGNMENT];
	stream->Write( &tmpByte, 1);
	stream->Write( filling,4); //this is called ID in iwd2, and contains 2 words
	stream->Write( actor->GetScriptName(), 32);
	return 0;
}

int CREImp::PutActorPST(DataStream *stream, Actor *actor)
{
	ieByte tmpByte;
	ieWord tmpWord;
	int i;
	char filling[44];

	memset(filling,0,sizeof(filling));
	stream->Write(filling, 44); //11*4 totally unknown
	stream->WriteDword( &actor->BaseStats[IE_XP_MAGE]);
	stream->WriteDword( &actor->BaseStats[IE_XP_THIEF]);
	for (i = 0; i<10; i++) {
		tmpWord = actor->BaseStats[IE_INTERNAL_0];
		stream->WriteWord( &tmpWord );
	}
	stream->Write(filling,4); //unknown
	stream->Write(actor->KillVar, 32);
	stream->Write(filling,3); //unknown
	tmpByte=actor->BaseStats[IE_COLORCOUNT];
	stream->Write( &tmpByte, 1);
	stream->WriteWord( &actor->AppearanceFlags1);
	stream->WriteWord( &actor->AppearanceFlags2);
	for (i=0;i<7;i++) {
		tmpWord = actor->BaseStats[IE_COLORS+i];
		stream->WriteWord( &tmpWord);
	}
	stream->Write(filling,31);
	tmpByte = actor->BaseStats[IE_SPECIES];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_TEAM];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_FACTION];
	stream->Write( &tmpByte, 1);
	//similar in all engines
	tmpByte = actor->BaseStats[IE_EA];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_GENERAL];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RACE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_CLASS];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SPECIFIC];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SEX];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 5); //unknown bytes
	tmpByte = actor->BaseStats[IE_ALIGNMENT];
	stream->Write( &tmpByte, 1);
	stream->Write( filling,4); //this is called ID in iwd2, and contains 2 words
	stream->Write( actor->GetScriptName(), 32);
	return 0;
}

int CREImp::PutActorIWD1(DataStream *stream, Actor *actor)
{
	ieByte tmpByte;
	ieWord tmpWord;
	int i;
	char filling[52];

	memset(filling,0,sizeof(filling));
	tmpByte=(ieByte) actor->BaseStats[IE_AVATARREMOVAL];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 3); //various scripting flags currently down the drain
	for (i=0;i<5;i++) {
		tmpWord = actor->BaseStats[IE_INTERNAL_0+i];
		stream->WriteWord( &tmpWord);
	}
	stream->Write( actor->KillVar, 32); //some variable names in iwd
	stream->Write( actor->KillVar, 32); //some variable names in iwd
	stream->Write( filling, 2);
	tmpWord = actor->BaseStats[IE_SAVEDXPOS];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_SAVEDYPOS];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_SAVEDFACE];
	stream->WriteWord( &tmpWord);
	stream->Write( filling, 18);
	//similar in all engines
	tmpByte = actor->BaseStats[IE_EA];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_GENERAL];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RACE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_CLASS];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SPECIFIC];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SEX];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 5); //unknown bytes
	tmpByte = actor->BaseStats[IE_ALIGNMENT];
	stream->Write( &tmpByte, 1);
	stream->Write( filling,4); //this is called ID in iwd2, and contains 2 words
	stream->Write( actor->GetScriptName(), 32);
	return 0;
}

int CREImp::PutActorIWD2(DataStream *stream, Actor *actor)
{
	ieByte tmpByte;
	ieWord tmpWord;
	int i;
	char filling[146];

	memset(filling,0,sizeof(filling));
	tmpByte=(ieByte) actor->BaseStats[IE_AVATARREMOVAL];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 3); //various scripting flags currently down the drain
	for (i=0;i<5;i++) {
		tmpWord = actor->BaseStats[IE_INTERNAL_0+i];
		stream->WriteWord( &tmpWord);
	}
	stream->Write( actor->KillVar, 32); //some variable names in iwd
	stream->Write( actor->KillVar, 32); //some variable names in iwd
	stream->Write( filling, 2);
	tmpWord = actor->BaseStats[IE_SAVEDXPOS];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_SAVEDYPOS];
	stream->WriteWord( &tmpWord);
	tmpWord = actor->BaseStats[IE_SAVEDFACE];
	stream->WriteWord( &tmpWord);
	stream->Write( filling, 146);
	//similar in all engines
	tmpByte = actor->BaseStats[IE_EA];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_GENERAL];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_RACE];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_CLASS];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SPECIFIC];
	stream->Write( &tmpByte, 1);
	tmpByte = actor->BaseStats[IE_SEX];
	stream->Write( &tmpByte, 1);
	stream->Write( filling, 5); //unknown bytes
	tmpByte = actor->BaseStats[IE_ALIGNMENT];
	stream->Write( &tmpByte, 1);
	stream->Write( filling,4); //this is called ID in iwd2, and contains 2 words
	stream->Write( actor->GetScriptName(), 32);
	//3 unknown words
	stream->WriteWord( &tmpWord);
	stream->WriteWord( &tmpWord);
	stream->WriteWord( &tmpWord);
	return 0;
}

int CREImp::PutKnownSpells( DataStream *stream, Actor *actor)
{
	int type=actor->spellbook.GetTypes();
	for (int i=0;i<type;i++) {
		unsigned int level = actor->spellbook.GetSpellLevelCount(i);
		for (unsigned int j=0;j<level;j++) {
			unsigned int count = actor->spellbook.GetKnownSpellsCount(i, j);
			for (unsigned int k=0;k<count;k++) {
				CREKnownSpell *ck = actor->spellbook.GetKnownSpell(i, j, k);
				stream->WriteResRef(ck->SpellResRef);
				stream->WriteWord( &ck->Level);
				stream->WriteWord( &ck->Type);
			}
		}
	}
	return 0;
}

int CREImp::PutSpellPages( DataStream *stream, Actor *actor)
{
	ieWord tmpWord;
	ieDword tmpDword;
	ieDword SpellIndex = 0;

	int type=actor->spellbook.GetTypes();
	for (int i=0;i<type;i++) {
		unsigned int level = actor->spellbook.GetSpellLevelCount(i);
		for (unsigned int j=0;j<level;j++) {
			tmpWord = j; //+1
			stream->WriteWord( &tmpWord);
			tmpWord = actor->spellbook.GetMemorizableSpellsCount(i,j,false);
			stream->WriteWord( &tmpWord);
			tmpWord = actor->spellbook.GetMemorizableSpellsCount(i,j,true);
			stream->WriteWord( &tmpWord);
			tmpWord = i;
			stream->WriteWord( &tmpWord);
			stream->WriteDword( &SpellIndex);
			tmpDword = actor->spellbook.GetMemorizedSpellsCount(i,j);
			stream->WriteDword( &tmpDword);
			SpellIndex += tmpDword;
		}
	}
	return 0;
}

int CREImp::PutMemorizedSpells(DataStream *stream, Actor *actor)
{
	int type=actor->spellbook.GetTypes();
	for (int i=0;i<type;i++) {
		unsigned int level = actor->spellbook.GetSpellLevelCount(i);
		for (unsigned int j=0;j<level;j++) {
			unsigned int count = actor->spellbook.GetMemorizedSpellsCount(i,j);
			for (unsigned int k=0;k<count;k++) {
				CREMemorizedSpell *cm = actor->spellbook.GetMemorizedSpell(i,j,k);

				stream->WriteResRef( cm->SpellResRef);
				stream->WriteDword( &cm->Flags);
			}
		}
	}
	return 0;
}

int CREImp::PutEffects( DataStream *stream, Actor *actor)
{
	ieDword tmpDword1,tmpDword2;
	ieWord tmpWord;
	ieByte tmpByte;
	char filling[60];

	memset(filling,0,sizeof(filling) );
	std::list< Effect* >::const_iterator f=actor->fxqueue.GetFirstEffect();
	for(unsigned int i=0;i<EffectsCount;i++) {
		const Effect *fx = actor->fxqueue.GetNextSavedEffect(f);

		assert(fx!=NULL);

		if (TotSCEFF) {
			stream->Write( filling,8 ); //signature
			stream->WriteDword( &fx->Opcode);
			stream->WriteDword( &fx->Target);
			stream->WriteDword( &fx->Power);
			stream->WriteDword( &fx->Parameter1);
			stream->WriteDword( &fx->Parameter2);
			stream->WriteDword( &fx->TimingMode);
			stream->WriteDword( &fx->Duration);
			stream->WriteWord( &fx->Probability1);
			stream->WriteWord( &fx->Probability2);
			stream->Write(fx->Resource, 8);
			stream->WriteDword( &fx->DiceThrown );
			stream->WriteDword( &fx->DiceSides );
			stream->WriteDword( &fx->SavingThrowType );
			stream->WriteDword( &fx->SavingThrowBonus );
			//isvariable
			stream->Write( filling,4 );
			stream->WriteDword( &fx->PrimaryType );
			stream->Write( filling,12 );
			stream->WriteDword( &fx->Resistance );
			stream->WriteDword( &fx->Parameter3 );
			stream->WriteDword( &fx->Parameter4 );
			stream->Write( filling,8 );
			if (fx->IsVariable) {
				stream->Write(fx->Resource+8, 8);
				stream->Write(fx->Resource+16, 8);
			} else {
				stream->Write(fx->Resource2, 8);
				stream->Write(fx->Resource3, 8);
			}
			tmpDword1 = (ieDword) fx->PosX;
			tmpDword2 = (ieDword) fx->PosY;
			stream->WriteDword( &tmpDword1 );
			stream->WriteDword( &tmpDword2 );
			stream->WriteDword( &tmpDword1 );
			stream->WriteDword( &tmpDword2 );
			stream->Write( filling,4 );
			stream->Write(fx->Source, 8);
			stream->Write( filling,4 );
			stream->WriteDword( &fx->Projectile );
			tmpDword1 = (ieDword) fx->InventorySlot;
			stream->WriteDword( &tmpDword1 );
			stream->Write( filling,40 ); //12+32+8
			stream->WriteDword( &fx->SecondaryType );
			stream->Write( filling,60 );
		} else {
			tmpWord = (ieWord) fx->Opcode;
			stream->WriteWord( &tmpWord);
			tmpByte = (ieByte) fx->Target;
			stream->Write(&tmpByte, 1);
			tmpByte = (ieByte) fx->Power;
			stream->Write(&tmpByte, 1);
			stream->WriteDword( &fx->Parameter1);
			stream->WriteDword( &fx->Parameter2);
			tmpByte = (ieByte) fx->TimingMode;
			stream->Write(&tmpByte, 1);
			tmpByte = (ieByte) fx->Resistance;
			stream->Write(&tmpByte, 1);
			stream->WriteDword( &fx->Duration);
			tmpByte = (ieByte) fx->Probability1;
			stream->Write(&tmpByte, 1);
			tmpByte = (ieByte) fx->Probability2;
			stream->Write(&tmpByte, 1);
			stream->Write(fx->Resource, 8);
			stream->WriteDword( &fx->DiceThrown );
			stream->WriteDword( &fx->DiceSides );
			stream->WriteDword( &fx->SavingThrowType );
			stream->WriteDword( &fx->SavingThrowBonus );
			//isvariable
			stream->Write( filling,4 );
		}
	}
	return 0;
}

//add as effect!
int CREImp::PutVariables( DataStream *stream, Actor *actor)
{
	char filling[104];
	POSITION pos=NULL;
	const char *name;
	ieDword tmpDword, value;

	for (unsigned int i=0;i<VariablesCount;i++) {
		memset(filling,0,sizeof(filling) );
		pos = actor->locals->GetNextAssoc( pos, name, value);
		stream->Write(filling,8);
		tmpDword = FAKE_VARIABLE_OPCODE;
		stream->WriteDword( &tmpDword);
		stream->Write(filling,8); //type, power
		stream->WriteDword( &value); //param #1
		stream->Write( filling, 40); //param #2, timing, duration, chance, resource, dices, saves
		tmpDword = FAKE_VARIABLE_MARKER;
		stream->WriteDword( &tmpDword); //variable marker
		stream->Write( filling, 92); //23 * 4
		strnspccpy(filling, name, 32);
		stream->Write( filling, 104); //32 + 72
	}
	return 0;
}

/* this function expects GetStoredFileSize to be called before */
int CREImp::PutActor(DataStream *stream, Actor *actor, bool chr)
{
	ieDword tmpDword=0;
	int ret;

	if (!stream || !actor) {
		return -1;
	}

	IsCharacter = chr;
	if (chr) {
		WriteChrHeader( stream, actor );
	}
	assert(TotSCEFF==0 || TotSCEFF==1);

	ret = PutHeader( stream, actor);
	if (ret) {
		return ret;
	}
	//here comes the fuzzy part
	ieDword Inventory_Size;

	switch (CREVersion) {
		case IE_CRE_GEMRB:
			//don't add fist
			Inventory_Size=(ieDword) actor->inventory.GetSlotCount()-1;
			ret = PutActorGemRB(stream, actor, Inventory_Size);
			break;
		case IE_CRE_V1_2:
			Inventory_Size=46;
			ret = PutActorPST(stream, actor);
			break;
		case IE_CRE_V1_1:
		case IE_CRE_V1_0: //bg1/bg2
			Inventory_Size=38;
			ret = PutActorBG(stream, actor);
			break;
		case IE_CRE_V2_2:
			Inventory_Size=50;
			ret = PutActorIWD2(stream, actor);
			break;
		case IE_CRE_V9_0:
			Inventory_Size=38;
			ret = PutActorIWD1(stream, actor);
			break;
		default:
			return -1;
	}
	if (ret) {
		return ret;
	}

	//writing offsets
	if (actor->version==IE_CRE_V2_2) {
		int i;

		//class spells
		for (i=0;i<7*9;i++) {
			stream->WriteDword(&KnownSpellsOffset);
			KnownSpellsOffset+=8;
		}
		for (i=0;i<7*9;i++) {
			stream->WriteDword(&tmpDword);
		}
		//domain spells
		for (i=0;i<9;i++) {
			stream->WriteDword(&KnownSpellsOffset);
			KnownSpellsOffset+=8;
		}
		for (i=0;i<9;i++) {
			stream->WriteDword(&tmpDword);
		}
		//innates, shapes, songs
		for (i=0;i<3;i++) {
			stream->WriteDword(&KnownSpellsOffset);
			KnownSpellsOffset+=8;
			stream->WriteDword(&tmpDword);
		}
	} else {
		stream->WriteDword( &KnownSpellsOffset);
		stream->WriteDword( &KnownSpellsCount);
		stream->WriteDword( &SpellMemorizationOffset );
		stream->WriteDword( &SpellMemorizationCount );
		stream->WriteDword( &MemorizedSpellsOffset );
		stream->WriteDword( &MemorizedSpellsCount );
	}
	stream->WriteDword( &ItemSlotsOffset );
	stream->WriteDword( &ItemsOffset );
	stream->WriteDword( &ItemsCount );
	stream->WriteDword( &EffectsOffset );
	tmpDword = EffectsCount+VariablesCount;
	stream->WriteDword( &tmpDword );
	stream->WriteResRef( actor->GetDialog(false) );
	//spells, spellbook etc

	if (actor->version==IE_CRE_V2_2) {
		int i;

		//putting out book headers
		for (i=0;i<7*9;i++) {
			stream->WriteDword(&tmpDword);
			stream->WriteDword(&tmpDword);
		}
		//domain headers
		for (i=0;i<9;i++) {
			stream->WriteDword(&tmpDword);
			stream->WriteDword(&tmpDword);
		}
		//innates, shapes, songs
		for (i=0;i<3;i++) {
			stream->WriteDword(&tmpDword);
			stream->WriteDword(&tmpDword);
		}
	} else {
		ret = PutKnownSpells( stream, actor);
		if (ret) {
			return ret;
		}
		ret = PutSpellPages( stream, actor);
		if (ret) {
			return ret;
		}
		ret = PutMemorizedSpells( stream, actor);
		if (ret) {
			return ret;
		}
	}
	ret = PutEffects(stream, actor);
	if (ret) {
		return ret;
	}
	//effects and variables
	ret = PutVariables(stream, actor);
	if (ret) {
		return ret;
	}

	//items and inventory slots
	ret = PutInventory( stream, actor, Inventory_Size);
	if (ret) {
		return ret;
	}
	return 0;
}
