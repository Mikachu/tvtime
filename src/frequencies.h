/*
 * Worldwide channel/frequency list
 *
 * Nathan Laredo (laredo@gnu.org)
 *
 * Modified 11 November 2002, Billy Biggs
 *     Added S04, S05 and E2A to BG-CATV
 *     Added macros for IRC and HRC NTSC cable
 *     Added the rest of the western europe broadcast frequencies
 *     Added channel 12 for Australia
 *     Added eastern europe/russia/china cable frequencies
 *     Added france frequencies
 *     Added argentina cable frequencies
 * Modified 19 August 1998, Nathan Laredo
 *     Removed NTSC UHF broadcast 70-83 per FCC request
 *
 * All frequencies are given in kHz 
 */
#define NTSC_AUDIO_CARRIER	4500
#define PAL_AUDIO_CARRIER_I	6000
#define PAL_AUDIO_CARRIER_BGHN	5500
#define PAL_AUDIO_CARRIER_MN	4500
#define PAL_AUDIO_CARRIER_D	6500
#define SEACAM_AUDIO_DKK1L	6500
#define SEACAM_AUDIO_BG		5500
/* NICAM 728 32-kHz, 14-bit digital stereo audio is transmitted in 1ms frames
   containing 8 bits frame sync, 5 bits control, 11 bits additional data, and
   704 bits audio data.  The bit rate is reduced by transmitting only 10 bits
   plus parity of each 14 bit sample, the largest sample in a frame determines
   which 10 bits are transmitted.  The parity bits for audio samples also 
   specify the scaling factor used for that channel during that frame.  The
   companeded audio data is interleaved to reduce the influence of dropouts
   and the whole frame except for sync bits is scrambled for spectrum shaping.
   Data is modulated using QPSK, at below following subcarrier freqs */
#define NICAM728_PAL_BGH	5850
#define NICAM728_PAL_I		6552

/* COMPREHENSIVE LIST OF FORMAT BY COUNTRY
   (M) NTSC used in:
	Antigua, Aruba, Bahamas, Barbados, Belize, Bermuda, Bolivia, Burma,
	Canada, Chile, Colombia, Costa Rica, Cuba, Curacao, Dominican Republic,
	Ecuador, El Salvador, Guam Guatemala, Honduras, Jamaica, Japan,
	South Korea, Mexico, Montserrat, Myanmar, Nicaragua, Panama, Peru,
	Philippines, Puerto Rico, St Christopher and Nevis, Samoa, Suriname,
	Taiwan, Trinidad/Tobago, United States, Venezuela, Virgin Islands,
        Soviet Canuckistan
   (B) PAL used in:
	Albania, Algeria, Australia, Austria, Bahrain, Bangladesh, Belgium,
	Bosnia-Herzegovinia, Brunei Darussalam, Cambodia, Cameroon, Croatia,
	Cyprus, Denmark, Egypt, Ethiopia, Equatorial Guinea, Finland, Germany,
	Ghana, Gibraltar, Greenland, Iceland, India, Indonesia, Israel, Italy,
	Jordan, Kenya, Kuwait, Liberia, Libya, Luxembourg, Malaysa, Maldives,
	Malta, Nepal, Netherlands, New Zealand, Nigeria, Norway, Oman, Pakistan,
	Papua New Guinea, Portugal, Qatar, Sao Tome and Principe, Saudi Arabia,
	Seychelles, Sierra Leone, Singapore, Slovenia, Somali, Spain,
	Sri Lanka, Sudan, Swaziland, Sweden, Switzeland, Syria, Thailand,
	Tunisia, Turkey, Uganda, United Arab Emirates, Yemen
   (N) PAL used in: (Combination N = 4.5MHz audio carrier, 3.58MHz burst)
	Argentina (Combination N), Paraguay, Uruguay
   (M) PAL (525/60, 3.57MHz burst) used in:
	Brazil
   (G) PAL used in:
	Albania, Algeria, Austria, Bahrain, Bosnia/Herzegovinia, Cambodia,
	Cameroon, Croatia, Cyprus, Denmark, Egypt, Ethiopia, Equatorial Guinea,
	Finland, Germany, Gibraltar, Greenland, Iceland, Israel, Italy, Jordan,
	Kenya, Kuwait, Liberia, Libya, Luxembourg, Malaysia, Monaco,
	Mozambique, Netherlands, New Zealand, Norway, Oman, Pakistan,
	Papa New Guinea, Portugal, Qatar, Romania, Sierra Leone, Singapore,
	Slovenia, Somalia, Spain, Sri Lanka, Sudan, Swaziland, Sweeden,
	Switzerland, Syria, Thailand, Tunisia, Turkey, United Arab Emirates,
	Yemen, Zambia, Zimbabwe
   (D) PAL used in:
	China, North Korea, Romania
   (H) PAL used in:
	Belgium
   (I) PAL used in:
	Angola, Botswana, Gambia, Guinea-Bissau, Hong Kong, Ireland, Lesotho,
	Malawi, Nambia, Nigeria, South Africa, Tanzania, United Kingdom,
	Zanzibar
   (B) SECAM used in:
	Djibouti, Greece, Iran, Iraq, Lebanon, Mali, Mauritania, Mauritus,
	Morocco
   (D) SECAM used in:
	Afghanistan, Armenia, Azerbaijan, Belarus, Bulgaria, Czech Republic,
	Estonia, Georgia, Hungary, Kazakhstan, Lithuania, Mongolia, Moldova,
	Poland, Russia, Slovak Republic, Ukraine, Vietnam
   (G) SECAM used in:
	Greecem Iran, Iraq, Mali, Mauritus, Morocco, Saudi Arabia
   (K) SECAM used in:
	Armenia, Azerbaijan, Bulgaria, Czech Republic, Estonia, Georgia,
	Hungary, Kazakhstan, Lithuania, Madagascar, Moldova, Poland, Russia,
	Slovak Republic, Ukraine, Vietnam
   (K1) SECAM used in:
	Benin, Burkina Faso, Burundi, Chad, Cape Verde, Central African
	Republic, Comoros, Congo, Gabon, Madagascar, Niger, Rwanda, Senegal,
	Togo, Democratic Republic of Congo (formerly Zaire)
   (L) SECAM used in:
	France
*/
#define NTSC_BCAST    	0
#define NTSC_CABLE	1
#define NTSC_JP_BCAST	2
#define NTSC_JP_CABLE	3
#define PAL_EUROPE	4
#define PAL_ITALY	5
#define PAL_NEWZEALAND  6
#define PAL_AUSTRALIA	7
#define PAL_UHF_GHI	8
#define PAL_IRELAND	8
#define PAL_CABLE_BG	9
#define PAL_CABLE_NC    10
#define PAL_EAST_EUROPE 11
#define SECAM_FRANCE    12

#define NUM_FREQ_TABLES 13

struct freqlist {
  const char name[4];
  int freq[NUM_FREQ_TABLES];
};

struct table_info {
  const char *long_name;
  const char *short_name;
};

struct table_info freq_table_names[NUM_FREQ_TABLES] = {
   /* NTSC_BCAST      */ { "US broadcast", "us-broadcast" },
   /* NTSC_CABLE      */ { "US cable frequencies", "us-cable" },
   /* NTSC_JP_BCAST   */ { "Japan broadcast", "japan-broadcast" },
   /* NTSC_JP_CABLE   */ { "Japan cable", "japan-cable" },
   /* PAL_EUROPE      */ { "Western europe broadcast", "europe-west" },
   /* PAL_ITALY       */ { "Italy", "italy" },
   /* PAL_NEWZEALAND  */ { "New Zealand", "newzealand" },
   /* PAL_AUSTRALIA   */ { "Australia", "australia" },
   /* PAL_UHF_GHI     */ { "UK broadcast", "uk-broadcast" },
   /* PAL_CABLE_BG    */ { "Europe cable", "europe-cable" },
   /* PAL_CABLE_NC    */ { "Argentina cable", "argentina-cable" },
   /* PAL_EAST_EUROPE */ { "Eastern europe", "europe-east" },
   /* SECAM_FRANCE    */ { "France", "france" }
};

struct freqlist tvtuner[] = {
/* CH  US-TV  US-CATV JP-TV JP-CATV EUROPE  ITALY  NZ     AU   UHF_GHI BGCATV NCCATV EASTEU FRANCE */
{"S01",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 69250,     0,     0,     0}},
{"S02",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 76250,     0,     0,     0}},
{"S03",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 83250,     0,     0,     0}},
{"S04",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 90250,     0,     0,     0}},
{"S05",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 97250,     0,     0,     0}},
{" E2",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 48250,     0,     0,     0}},
{"E2A",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 49750,     0,     0,     0}},
{" E3",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 55250,     0,     0,     0}},
{" E4",{     0,     0,     0,     0,     0,     0,     0,     0,     0, 62250,     0,     0,     0}},
{" E5",{     0,     0,     0,     0,     0,     0,     0,     0,     0,175250,     0,     0,     0}},
{" E6",{     0,     0,     0,     0,     0,     0,     0,     0,     0,182250,     0,     0,     0}},
{" E7",{     0,     0,     0,     0,     0,     0,     0,     0,     0,189250,     0,     0,     0}},
{" E8",{     0,     0,     0,     0,     0,     0,     0,     0,     0,196250,     0,     0,     0}},
{" E9",{     0,     0,     0,     0,     0,     0,     0,     0,     0,203250,     0,     0,     0}},
{"E10",{     0,     0,     0,     0,     0,     0,     0,     0,     0,210250,     0,     0,     0}},
{"E11",{     0,     0,     0,     0,     0,     0,     0,     0,     0,217250,     0,     0,     0}},
{"E12",{     0,     0,     0,     0,     0,     0,     0,     0,     0,224250,     0,     0,     0}},
{"  0",{     0,     0,     0,     0,     0,     0,     0, 46250, 45750,     0,     0,     0,     0}},
{"  1",{     0, 73250, 91250,     0,     0,     0, 45250, 57250, 53750,105250, 56250, 49750, 47750}},
{"  2",{ 55250, 55250, 97250,     0, 48250, 53750, 55250, 64250, 61750,112250, 62250, 57250, 55750}},
{"  3",{ 61250, 61250,103250,     0, 55250, 62250, 62250, 86250,175250,119250, 68250, 65250, 60500}},
{"  4",{ 67250, 67250,171250,     0, 62250, 82250,175250, 95250,183250,126250, 74250, 77250,176000}},
{"  5",{ 77250, 77250,177250,     0,175250,175250,182250,102250,191250,133250, 84250, 85250,184000}},
{" 5A",{     0,     0,     0,     0,     0,     0,     0,138250,     0,     0,     0,     0,     0}},
{"  6",{ 83250, 83250,183250,     0,182250,183750,189250,175250,199250,140250, 90250,168250,192000}},
{"  7",{175250,175250,189250,     0,189250,192250,196250,182250,207250,147250,182250,176250,200000}},
{"  8",{181250,181250,193250,     0,196250,201250,203250,189250,215250,154250,188250,184250,208000}},
{"  9",{187250,187250,199250,     0,203250,210250,210250,196250,     0,161250,194250,192250,216000}},
{" 10",{193250,193250,205250,     0,210250,210250,217250,209250,     0,168250,200250,200250,     0}},
{" 11",{199250,199250,211250,     0,217250,217250,     0,216250,     0,231250,206250,208250,     0}},
{" 12",{205250,205250,217250,     0,224250,224250,     0,223250,     0,238250,212250,216250,     0}},
{" 13",{211250,211250,     0,109250,     0,     0,     0,     0,     0,245250,218250,471250,     0}},
{" 14",{471250,121250,     0,115250,     0,     0,     0,     0,     0,252250,128250,479250,     0}},
{" 15",{477250,127250,     0,121250,     0,     0,     0,     0,     0,259250,134250,487250,     0}},
{" 16",{483250,133250,     0,127250,     0,     0,     0,     0,     0,266250,140000,493250,     0}},
{" 17",{489250,139250,     0,133250,     0,     0,     0,     0,     0,273250,146250,503250,     0}},
{" 18",{495250,145250,     0,139250,     0,     0,     0,     0,     0,280250,152250,511250,     0}},
{" 19",{501250,151250,     0,145250,     0,     0,     0,     0,     0,287250,158250,519250,     0}},
{" 20",{507250,157250,     0,151250,     0,     0,     0,     0,     0,294250,164250,527250,     0}},
{" 21",{513250,163250,     0,157250,471250,     0,     0,     0,471250,303250,170250,535250,471250}},
{" 22",{519250,169250,     0,165250,479250,     0,     0,     0,479250,311250,176250,543250,479250}},
{" 23",{525250,217250,     0,223250,487250,     0,     0,     0,487250,319250,224250,551250,487250}},
{" 24",{531250,223250,     0,231250,495250,     0,     0,     0,495250,327250,230250,559250,495250}},
{" 25",{537250,229250,     0,237250,503250,     0,     0,     0,503250,335250,236250,607250,503250}},
{" 26",{543250,235250,     0,243250,511250,     0,     0,     0,511250,343250,242250,615250,511250}},
{" 27",{549250,241250,     0,249250,519250,     0,     0,     0,519250,351250,248250,623250,519250}},
{" 28",{555250,247250,     0,253250,527250,     0,     0,     0,527250,359250,254250,631250,527250}},
{" 29",{561250,253250,     0,259250,525250,     0,     0,     0,525250,367250,260250,639250,535250}},
{" 30",{567250,259250,     0,265250,543250,     0,     0,     0,543250,375250,266250,647250,543250}},
{" 31",{573250,265250,     0,271250,551250,     0,     0,     0,551250,383250,272250,655250,551250}},
{" 32",{579250,271250,     0,277250,559250,     0,     0,     0,559250,391250,278250,663250,559250}},
{" 33",{585250,277250,     0,283250,567250,     0,     0,     0,567250,399250,284250,671250,567250}},
{" 34",{591250,283250,     0,289250,575250,     0,     0,     0,575250,407250,290250,679250,575250}},
{" 35",{597250,289250,     0,295250,583250,     0,     0,     0,583250,415250,296250,687250,583250}},
{" 36",{603250,295250,     0,301250,591250,     0,     0,     0,591250,423250,302250,695250,591250}},
{" 37",{609250,301250,     0,307250,599250,     0,     0,     0,599250,431250,308250,703250,599250}},
{" 38",{615250,307250,     0,313250,607250,     0,     0,     0,607250,439250,314250,711250,607250}},
{" 39",{621250,313250,     0,319250,615250,     0,     0,     0,615250,447250,320250,719250,615250}},
{" 40",{627250,319250,     0,325250,623250,     0,     0,     0,623250,455250,326250,727250,623250}},
{" 41",{633250,325250,     0,331250,631250,     0,     0,     0,631250,463250,332250,735250,631250}},
{" 42",{639250,331250,     0,337250,639250,     0,     0,     0,639250,     0,338250,743250,639250}},
{" 43",{645250,337250,     0,343250,647250,     0,     0,     0,647250,     0,344250,751250,647250}},
{" 44",{651250,343250,     0,349250,655250,     0,     0,     0,655250,     0,350250,759250,655250}},
{" 45",{657250,349250,663250,355250,663250,     0,     0,     0,663250,     0,356250,767250,663250}},
{" 46",{663250,355250,669250,361250,671250,     0,     0,     0,671250,     0,362250,775250,671250}},
{" 47",{669250,361250,675250,367250,679250,     0,     0,     0,679250,     0,368250,783250,679250}},
{" 48",{675250,367250,681250,373250,687250,     0,     0,     0,687250,     0,374250,791250,687250}},
{" 49",{681250,373250,687250,379250,695250,     0,     0,     0,695250,     0,380250,799250,695250}},
{" 50",{687250,379250,693250,385250,703250,     0,     0,     0,703250,     0,386250,807250,703250}},
{" 51",{693250,385250,699250,391250,711250,     0,     0,     0,711250,     0,392250,815250,711250}},
{" 52",{699250,391250,705250,397250,719250,     0,     0,     0,719250,     0,398250,823250,719250}},
{" 53",{705250,397250,711250,403250,727250,     0,     0,     0,727250,     0,404250,831250,727250}},
{" 54",{711250,403250,717250,409250,735250,     0,     0,     0,735250,     0,410250,839250,735250}},
{" 55",{717250,409250,723250,415250,743250,     0,     0,     0,743250,     0,416250,847250,743250}},
{" 56",{723250,415250,729250,421250,751250,     0,     0,     0,751250,     0,422250,855250,751250}},
{" 57",{729250,421250,735250,427250,759250,     0,     0,     0,759250,     0,428250,863250,759250}},
{" 58",{735250,427250,741250,433250,767250,     0,     0,     0,767250,     0,434250,     0,767250}},
{" 59",{741250,433250,747250,439250,775250,     0,     0,     0,775250,     0,440250,     0,775250}},
{" 60",{747250,439250,753250,445250,783250,     0,     0,     0,783250,     0,446250,     0,783250}},
{" 61",{753250,445250,759250,451250,791250,     0,     0,     0,791250,     0,452250,     0,791250}},
{" 62",{759250,451250,765250,457250,799250,     0,     0,     0,799250,     0,458250,     0,799250}},
{" 63",{765250,457250,     0,463250,807250,     0,     0,     0,807250,     0,464250,     0,807250}},
{" 64",{771250,463250,     0,     0,815250,     0,     0,     0,815250,     0,470250,     0,815250}},
{" 65",{777250,469250,     0,     0,823250,     0,     0,     0,823250,     0,476250,     0,823250}},
{" 66",{783250,475250,     0,     0,831250,     0,     0,     0,831250,     0,482250,     0,831250}},
{" 67",{789250,481250,     0,     0,839250,     0,     0,     0,839250,     0,488250,     0,839250}},
{" 68",{795250,487250,     0,     0,847250,     0,     0,     0,847250,     0,494250,     0,847250}},
{" 69",{801250,493250,     0,     0,855250,     0,     0,     0,855250,     0,500250,     0,855250}},
{" 70",{     0,499250,     0,     0,     0,     0,     0,     0,     0,     0,506250,     0,     0}},
{" 71",{     0,505250,     0,     0,     0,     0,     0,     0,     0,     0,512250,     0,     0}},
{" 72",{     0,511250,     0,     0,     0,     0,     0,     0,     0,     0,518250,     0,     0}},
{" 73",{     0,517250,     0,     0,     0,     0,     0,     0,     0,     0,524250,     0,     0}},
{" 74",{     0,523250,     0,     0,     0,     0,     0,     0,     0,     0,530250,     0,     0}},
{" 75",{     0,529250,     0,     0,     0,     0,     0,     0,     0,     0,536250,     0,     0}},
{" 76",{     0,535250,     0,     0,     0,     0,     0,     0,     0,     0,542250,     0,     0}},
{" 77",{     0,541250,     0,     0,     0,     0,     0,     0,     0,     0,548250,     0,     0}},
{" 78",{     0,547250,     0,     0,     0,     0,     0,     0,     0,     0,554250,     0,     0}},
{" 79",{     0,553250,     0,     0,     0,     0,     0,     0,     0,     0,560250,     0,     0}},
{" 80",{     0,559250,     0,     0,     0,     0,     0,     0,     0,     0,566250,     0,     0}},
{" 81",{     0,565250,     0,     0,     0,     0,     0,     0,     0,     0,572250,     0,     0}},
{" 82",{     0,571250,     0,     0,     0,     0,     0,     0,     0,     0,578250,     0,     0}},
{" 83",{     0,577250,     0,     0,     0,     0,     0,     0,     0,     0,584250,     0,     0}},
{" 84",{     0,583250,     0,     0,     0,     0,     0,     0,     0,     0,590250,     0,     0}},
{" 85",{     0,589250,     0,     0,     0,     0,     0,     0,     0,     0,596250,     0,     0}},
{" 86",{     0,595250,     0,     0,     0,     0,     0,     0,     0,     0,602250,     0,     0}},
{" 87",{     0,601250,     0,     0,     0,     0,     0,     0,     0,     0,608250,     0,     0}},
{" 88",{     0,607250,     0,     0,     0,     0,     0,     0,     0,     0,614250,     0,     0}},
{" 89",{     0,613250,     0,     0,     0,     0,     0,     0,     0,     0,620250,     0,     0}},
{" 90",{     0,619250,     0,     0,     0,     0,     0,     0,     0,     0,626250,     0,     0}},
{" 91",{     0,625250,     0,     0,     0,     0,     0,     0,     0,     0,632250,     0,     0}},
{" 92",{     0,631250,     0,     0,     0,     0,     0,     0,     0,     0,638250,     0,     0}},
{" 93",{     0,637250,     0,     0,     0,     0,     0,     0,     0,     0,644250,     0,     0}},
{" 94",{     0,643250,     0,     0,     0,     0,     0,     0,     0,     0,116000,     0,     0}},
{" 95",{     0, 91250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" 96",{     0, 97250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" 97",{     0,103250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" 98",{     0,109250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" 99",{     0,115250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"100",{     0,649250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"101",{     0,655250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"102",{     0,661250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"103",{     0,667250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"104",{     0,673250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"105",{     0,679250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"106",{     0,685250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"107",{     0,691250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"108",{     0,697250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"109",{     0,703250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"110",{     0,709250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"111",{     0,715250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"112",{     0,721250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"113",{     0,727250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"114",{     0,733250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"115",{     0,739250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"116",{     0,745250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"117",{     0,751250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"118",{     0,757250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"119",{     0,763250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"120",{     0,769250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"121",{     0,775250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"122",{     0,781250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"123",{     0,787250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"124",{     0,793250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"125",{     0,799250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" T7",{     0,  8250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" T8",{     0, 14250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{" T9",{     0, 20250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"T10",{     0, 26250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"T11",{     0, 32250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"T12",{     0, 38250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}},
{"T13",{     0, 44250,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0}}
/* if CHAN_ENTRIES is changed, /usr/share/tv/tvset.rc will be invalid */
#define CHAN_ENTRIES 151
};

#define NTSC_CABLE_HRC(x) ((x == 77250) ? 78000 : ((x == 83250) ? 84000 : (x - 1250)))
#define NTSC_CABLE_IRC(x) ((x == 77250) ? 79250 : ((x == 83250) ? 85250 : x))

