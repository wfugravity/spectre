// Distributed under the MIT License.
// See LICENSE.txt for details.

#include "Framework/TestingFramework.hpp"

#include "DataStructures/Matrix.hpp"
#include "Evolution/Ader/Matrices.hpp"
#include "Framework/TestHelpers.hpp"
#include "NumericalAlgorithms/Spectral/Basis.hpp"
#include "NumericalAlgorithms/Spectral/Quadrature.hpp"
#include "NumericalAlgorithms/Spectral/Spectral.hpp"

namespace {
void test_predictor_inverse_temporal_matrix() {
  // The expected matrices were computed using a python implementation.
  std::vector<Matrix> expected_matrices{
      Matrix{{0.9999999999704987, -1.0000000000058982},
             {1.0000000000058982, 1.0000000000245408}},

      Matrix{{0.3333333333341427, -0.6666666666631862, 0.3333333333343741},
             {0.3333333333338683, 0.8333333333302344, -0.16666666666642072},
             {0.33333333333460546, 1.3333333333330097, 0.33333333333414294}},

      Matrix{{0.16666666666598579, -0.3726779962463638, 0.37267799624435705,
              -0.16666666666684973},
             {0.16666666666511903, 0.4999999999988805, -0.18841586141621497,
              0.07453559925100184},
             {0.16666666666542224, 0.8550825280846519, 0.5000000000009217,
              -0.07453559925086811},
             {0.16666666666762078, 0.8333333333457615, 0.8333333333430486,
              0.1666666666665209}},

      Matrix{{0.09999999999999079, -0.2333333333296262, 0.26666666666992644,
              -0.23333333332996198, 0.09999999999984038},
             {0.100000000000526, 0.32222222222439767, -0.1380230820629782,
              0.10400433198659419, -0.04285714285783839},
             {0.10000000000046355, 0.5626183666492172, 0.4055555555563216,
              -0.10567392219980562, 0.03749999999972064},
             {0.10000000000049575, 0.5404401124615488, 0.7348484788858046,
              0.3222222222235387, -0.04285714285777674},
             {0.09999999999969783, 0.544444444443468, 0.7111111111063032,
              0.5444444444436326, 0.09999999999999079}},

      Matrix{{0.06666666666700752, -0.1588447788016382, 0.19232929696085765,
              -0.19232929695910791, 0.15884477880415804, -0.0666666666668178},
             {0.0666666666670148, 0.22257081148232563, -0.10098685881827471,
              0.08649299947571226, -0.0677787383458266, 0.027979795608879355},
             {0.06666666666687401, 0.39229919084571846, 0.310762521853379,
              -0.09084920001658008, 0.058997819177094096, -0.02310851500410144},
             {0.06666666666730482, 0.374537141438732, 0.5790409103889227,
              0.3107625218493618, -0.0688842388598224, 0.02310851500329925},
             {0.06666666666824704, 0.37958702798528104, 0.5490853938498073,
              0.5751252195813528, 0.22257081148550922, -0.027979795609447904},
             {0.0666666666678089, 0.37847495630399225, 0.5548583770442755,
              0.5548583770377694, 0.3784749562994088, 0.06666666666599512}},

      Matrix{{0.04761904761897748, -0.11481373058704851, 0.14338515915935313,
              -0.15238095238129198, 0.14338515915910327, -0.1148137305869836,
              0.04761904761908773},
             {0.04761904761938801, 0.16222254749140472, -0.07595803696104785,
              0.06903032288224813, -0.06139870799999678, 0.048010952613251855,
              -0.019750021923025506},
             {0.04761904761931034, 0.28739843169335644, 0.23968221441509313,
              -0.07412324243657953, 0.05412775052222192, -0.03936755872370881,
              0.015814563441896915},
             {0.04761904761957719, 0.2735161565271106, 0.4525673743267387,
              0.2676190476187939, -0.06562985535159956, 0.039189181645760046,
              -0.014880952380883671},
             {0.0476190476192627, 0.27806329653279865, 0.4252366783060526,
              0.5111356870381575, 0.23968221441373858, -0.04870269388408819,
              0.01581456344192448},
             {0.04761904761949576, 0.2764341423701684, 0.4336750284379777,
              0.4817887948895896, 0.4482343573993073, 0.16222254749163476,
              -0.019750021923036633},
             {0.04761904761927915, 0.27682604736289684, 0.4317453812093726,
              0.4876190476158419, 0.4317453812098045, 0.2768260473622704,
              0.04761904761897753}},

      Matrix{{0.03571428571368002, -0.08674762803588493, 0.11037641642131896,
              -0.12136997667797732, 0.12136997667092636, -0.11037641642704883,
              0.08674762803220293, -0.03571428571430074},
             {0.03571428571481557, 0.12320925643128415, -0.058809657980279706,
              0.05527211524897814, -0.052172360298914255, 0.04621889588679081,
              -0.035876373195608324, 0.014703689691768454},
             {0.035714285714509166, 0.21896093735437683, 0.1884184890982874,
              -0.060215849730803246, 0.046518086216605, -0.038088555372400336,
              0.028548428326495918, -0.01155600304294998},
             {0.035714285714654806, 0.20799483956744333, 0.35844468875618307,
              0.2240865401892067, -0.057869269313596594, 0.038472630540354325,
              -0.026652206226434463, 0.010509272878307713},
             {0.03571428571443702, 0.21183010752353915, 0.33512938657112673,
              0.4346137782569374, 0.22408654018289773, -0.04980132089670387,
              0.028235713423931264, -0.010509272877884083},
             {0.035714285714600745, 0.21022456271681644, 0.3434969621429396,
              0.40521215939498245, 0.4334031934341878, 0.18841848910226483,
              -0.03632547410913365, 0.011556003043384265},
             {0.03571428571445321, 0.21086631462552144, 0.3403461327372401,
              0.41466273366009015, 0.40715510070658134, 0.3544900143287909,
              0.12320925642840279, -0.014703689691358351},
             {0.035714285714217336, 0.2107042271428155, 0.34112269248168114,
              0.4124587946546912, 0.4124587946536921, 0.34112269248400273,
              0.21070422714244116, 0.0357142857147142}},

      Matrix{
          {0.027777777777904966, -0.06780186853265521, 0.08732740318801908,
           -0.09809696322125326, 0.10158730158816573, -0.09809696322249856,
           0.08732740318589581, -0.06780186853309741, 0.027777777777700685},
          {0.02777777777791321, 0.0966365696695365, -0.046712218359015796,
           0.044842610594161336, -0.04380703757628316, 0.04116365452614564,
           -0.036132134919363276, 0.02785307304703796, -0.01138029017430888},
          {0.027777777778017054, 0.1720871253965892, 0.1511582451407597,
           -0.04933845587912496, 0.03939790154594345, -0.03412864478626619,
           0.028804902061080596, -0.021780901782210788, 0.00883577101944848},
          {0.027777777777333812, 0.1632724306350937, 0.2889104022893572,
           0.18710314437337683, -0.049867149899055184, 0.03493339413795813,
           -0.02704637147984173, 0.01966464558520257, -0.007865737255385497},
          {0.027777777777190985, 0.1664698807289659, 0.2693037380120288,
           0.3661044664383944, 0.19964852607839942, -0.046499343849742,
           0.029113561291716536, -0.01951409259904683, 0.007595486111221578},
          {0.027777777776984206, 0.16502993656987985, 0.27685688526085106,
           0.339272894607053, 0.39262029945338117, 0.1871031443737516,
           -0.039099888504309246, 0.02142215152179238, -0.007865737255285627},
          {0.027777777776952575, 0.16570931907622902, 0.2735115882091492,
           0.3493537127830036, 0.3644350496995086, 0.3645635238809584,
           0.1511582451405937, -0.028158708096811175, 0.008835771019663296},
          {0.02777777777686489, 0.16542006628597455, 0.27489364462744464,
           0.3454542450810375, 0.3737069650253814, 0.3417752890167973,
           0.2854737280766523, 0.09663656966959294, -0.011380290174234656},
          {0.027777777776708507, 0.16549536155439248, 0.2745387124899078,
           0.3464285109642727, 0.3715192743706812, 0.34642851096863286,
           0.27453871250042855, 0.16549536156063824, 0.02777777777790497}},

      Matrix{{0.022222222221906496, -0.054427523849716934, 0.07069328775990066,
              -0.08055952715622082, 0.0853150711139464, -0.0853150711186775,
              0.08055952715202327, -0.07069328776284448, 0.05442752384764724,
              -0.022222222222238845},
             {0.022222222222508053, 0.07776410653787505, -0.037920263982719374,
              0.036927735421106736, -0.03688110044165266, 0.035870233838581414,
              -0.033371150950317806, 0.029041632598949107,
              -0.022260437990038125, 0.009073114585841167},
             {0.022222222222298676, 0.13867457678353698, 0.12355578214264341,
              -0.040915456857439574, 0.03338414627744741, -0.029896601950691586,
              0.026692754011958673, -0.022720592684905134, 0.017214793613635156,
              -0.006985488666996244},
             {0.022222222222490935, 0.13146368216037643, 0.2368959169035338,
              0.15713245295282371, -0.042769039406889815, 0.03092016133532309,
              -0.02524262142739771, 0.020554926464678554, -0.015232616983223665,
              0.006129965976055818},
             {0.02222222222235726, 0.13413942728518716, 0.22038138305071794,
              0.3091930999779255, 0.1748809917005159, -0.04194883312265133,
              0.027569193013761623, -0.02052705637313009, 0.014598890364820791,
              -0.005788275787891686},
             {0.022222222222386626, 0.13288396881885056, 0.22700274737144358,
              0.2854570248876443, 0.347266372082993, 0.1748809917048405,
              -0.03813395052236873, 0.02292161008104799, -0.015010304765561245,
              0.005788275788091677},
             {0.022222222222203096, 0.13352485871349856, 0.2238350507907993,
              0.2950630828820245, 0.3201536315888052, 0.34677476884578357,
              0.15713245294851763, -0.03150721002937058, 0.01685605781234179,
              -0.006129965975854242},
             {0.0222222222220522, 0.133200325531795, 0.22538771252391648,
              0.2906735745398945, 0.33061782731949735, 0.3209741507094684,
              0.3076344956554706, 0.1235557821444346, -0.022477714228114308,
              0.006985488667226348},
             {0.022222222222032986, 0.13334420661771582, 0.2247110829768226,
              0.29252217268729763, 0.3265028310660932, 0.3295875578955742,
              0.28800661019426366, 0.233946232534877, 0.07776410653668284,
              -0.009073114585665199},
             {0.022222222221956987, 0.13330599084940398, 0.2248893420605291,
              0.29204268367337277, 0.3275397611764274, 0.3275397611779948,
              0.292042683675267, 0.22488934206468097, 0.13330599085097763,
              0.022222222222427496}},

      Matrix{
          {0.01818181818190092, -0.044642473306814805, 0.05833599883233813,
           -0.0671562769358682, 0.07222177017390477, -0.07388167388035201,
           0.07222177017371924, -0.06715627693638936, 0.058335998831632006,
           -0.044642473307234094, 0.01818181818179972},
          {0.018181818181884465, 0.0638970457245323, -0.03135603900725852,
           0.03084758641712686, -0.03128098998235485, 0.031114343070490727,
           -0.02995440928211618, 0.0276068082006394, -0.023855190205522774,
           0.018202618816853436, -0.007405022345450316},
          {0.01818181818205217, 0.11406140492110083, 0.10267584998255241,
           -0.03435447423964847, 0.028453895720033793, -0.026043682781403215,
           0.024038667801963173, -0.02163836439715823, 0.018444921784627883,
           -0.013970311904477681, 0.005666801272339664},
          {0.018181818182030893, 0.10806720291792335, 0.1972989428227929,
           0.13311496122315933, -0.036772860449869944, 0.02714975904172252,
           -0.022877487172695835, 0.019653114415352748, -0.016327680141672216,
           0.012199428064441502, -0.004922525897407123},
          {0.018181818181917025, 0.1103255422268642, 0.18329165188708757,
           0.2629369294006421, 0.15253047148107712, -0.03733312515688951,
           0.025273649287223258, -0.01978086529580089, 0.01568365984831898,
           -0.01144513703542942, 0.004577269590724688},
          {0.018181818181904563, 0.1092383670653025, 0.18905062359811212,
           0.24214296033922839, 0.304780237440625, 0.1591997068180093,
           -0.035674438915052786, 0.022431883951549464, -0.01623686652981973,
           0.011360139867655262, -0.004474431818129493},
          {0.01818181818190467, 0.10981867534922649, 0.18617229957461756,
           0.25092238842587977, 0.27978729367421024, 0.31895102195798775,
           0.15253047148095408, -0.03179540627042421, 0.018564307536025667,
           -0.0119520039130228, 0.00457726959073644},
          {0.01818181818197332, 0.10949930040746432, 0.18770373634797785,
           0.24657680803089058, 0.29020336758082194, 0.2930704813306262,
           0.30409874085815697, 0.13311496122317826, -0.025922886615722045,
           0.013631525554224474, -0.004922525897368947},
          {0.018181818181879545, 0.10966868382235812, 0.18690677817786133,
           0.24875559850311482, 0.28535011002786914, 0.30323427624102584,
           0.2809348821100695, 0.2614717083466195, 0.10267584998211361,
           -0.01836303300236528, 0.005666801272408237},
          {0.01818181818176121, 0.10959147263171942, 0.18726620636554275,
           0.24779245128544367, 0.287419323850867, 0.29919350191251687,
           0.2887459045512219, 0.244551673069504, 0.19476705516873852,
           0.06389704572454562, -0.007405022345380785},
          {0.018181818181636864, 0.10961227326601175, 0.18716988177784596,
           0.24804810426004392, 0.28687912477486166, 0.30021759545075083,
           0.2868791247750567, 0.24804810426082946, 0.18716988177962288,
           0.10961227326675485, 0.01818181818190091}},

      Matrix{
          {0.015151515151584232, -0.03727142812799102, 0.048923983331182815,
           -0.05674349752500363, 0.06170256160971506, -0.0641264424372233,
           0.06412644243863766, -0.06170256160829091, 0.05674349752595264,
           -0.04892398333066516, 0.03727142812829069, -0.015151515151509181},
          {0.01515151515156269, 0.053418016282432035, -0.026337819282717282,
           0.02610589416801112, -0.02676512444739308, 0.027042533469780965,
           -0.026626617339731436, 0.02538516813433355, -0.023212396318457083,
           0.019943687676434244, -0.01516349690472017, 0.006159367185928767},
          {0.0151515151515404, 0.09542754399508485, 0.08656311035886338,
           -0.029186317651895763, 0.02443801639291171, -0.02271304271947984,
           0.02142961206922456, -0.019938868676788502, 0.017966451862544062,
           -0.015299804406795028, 0.011574811128851692, -0.004692349145804841},
          {0.015151515151354199, 0.09037353349185437, 0.16660769069395492,
           0.11382996645635492, -0.03178950176515335, 0.0238229674736676,
           -0.020502170502257933, 0.01818197074102646, -0.015939020633403828,
           0.013355917722877841, -0.010014743777602848, 0.004045721913440676},
          {0.015151515151164962, 0.0922982058317993, 0.1546243868767208,
           0.22545960572983662, 0.1332135591757906, -0.03308324097512993,
           0.02285119371253928, -0.018429489372159796, 0.015376828409832734,
           -0.012535386915515763, 0.009262446732696578, -0.003720565328308077},
          {0.015151515150923093, 0.09135549819091596, 0.15963556234210327,
           0.20726230500011994, 0.26732630859823997, 0.14327837802934304,
           -0.03272425696964533, 0.021156361846805048, -0.016053057042708537,
           0.012473365089010743, -0.008994846794050059, 0.003579933684984922},
          {0.015151515150916147, 0.09187303429485524, 0.1570608851217073,
           0.21515436955776315, 0.2446980525460716, 0.2889779827279105,
           0.14327837803226115, -0.030629516600889424, 0.018653217997573468,
           -0.013220401434395387, 0.00913534912807098, -0.003579933685119333},
          {0.015151515151062427, 0.09157434237112415, 0.1584964506536467,
           0.21106537005542692, 0.2545535774183351, 0.26430076401179425,
           0.28874176507676125, 0.13321355917455283, -0.02688496867698337,
           0.01536396050764859, -0.009765960113496116, 0.003720565328180566},
          {0.015151515151322561, 0.09174713217719385, 0.15768235471670078,
           0.21329592324369012, 0.24956930523155096, 0.27478451969443596,
           0.2647051651686592, 0.2665894321970533, 0.11382996645817654,
           -0.02169655200643409, 0.01126311293344462, -0.00404572191358638},
          {0.015151515151495987, 0.09165248284445864, 0.15812299482041986,
           0.21211514584329877, 0.2521054943727682, 0.26983527023924914,
           0.2742586422525451, 0.24594656432223555, 0.2241215554759763,
           0.08656311035819617, -0.01528580314126821, 0.004692349145641109},
          {0.015151515151498793, 0.09169649916646774, 0.15791950920614115,
           0.21265354766206537, 0.2509736514656601, 0.27196329010955084,
           0.2704312756010187, 0.2529575112575845, 0.20946979001773183,
           0.16442403353067048, 0.05341801628305097, -0.006159367186092625},
          {0.015151515151414616, 0.09168451741272649, 0.1579747055645895,
           0.21250841776306503, 0.2512756032035713, 0.2714052409185767,
           0.2714052409196041, 0.25127560320440273, 0.212508417764308,
           0.15797470556436372, 0.09168451741363809, 0.015151515151370839}}};

  const auto perform_check =
      [&expected_matrices](const size_t num_points) {
        const Matrix& result = ader::dg::predictor_inverse_temporal_matrix<
            Spectral::Basis::Legendre, Spectral::Quadrature::GaussLobatto>(
            num_points);
        Approx custom_approx = Approx::custom().epsilon(1e-10);
        for (size_t i = 0; i < num_points; ++i) {
          for (size_t j = 0; j < num_points; ++j) {
            CHECK(expected_matrices[num_points - 2](i, j) ==
                  custom_approx(result(i, j)));
          }
        }
      };
  for (size_t i = Spectral::minimum_number_of_points<
           Spectral::Basis::Legendre, Spectral::Quadrature::GaussLobatto>;
       i <=
       std::min(Spectral::maximum_number_of_points<Spectral::Basis::Legendre>,
                12_st);

       /*The maximum number of allowed collocation points is given by
       `Spectral::maximum_number_of_points<Spectral::Basis::Legendre>`, but
       the matrices corresponding to numbers of points above 12 are not
       tested, because we only derived the expected matrices up to 12 points.*/
       ++i) {
    perform_check(i);
  }
}
}  // namespace

SPECTRE_TEST_CASE("Unit.Ader.Dg.Matrices", "[Unit][Ader]") {
  test_predictor_inverse_temporal_matrix();
}
