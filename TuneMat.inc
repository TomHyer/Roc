#include <iostream>
#include <memory>
#include <numeric>
#include <cmath>

using namespace std;

array<double, 6> ODD_VOLS = { 1000.0, 3000.0, 3000.0, 5000.0, 9000.0, 1000.0 };

namespace NNMat
{
array<array<double, 6>, 5> INIT_ODD = { {
        { 330.275543, 911.073898, 897.313399, 1565.859224, 2604.990052, 92.488155 },
        { 264.721609, 761.895357, 843.307318, 1236.708381, 2212.261871, 110.969674 },
        { 481.866475, 556.849155, 542.960773, 1088.422437, 1679.464728, -12.507642 },
        { -53.917841, 501.888041, 464.889870, 447.541050, 1448.730906, 59.966251 },
        { 288.426748, 84.470603, 134.150337, 325.265405, 268.637515, -40.247168 }
        } };

array<array<double, 42>, 6> INIT_EVEN = { {
        { 0.808287, 0.007073, 0.195386, -0.007205, 0.244094, 0.048363, 0.177037, -0.062350, 0.221346, -0.221375, 0.061489, 0.005377, 0.021075, 0.027289, -0.042046, 0.042308, -0.060377, 0.098275, -0.046728, -0.077107, -0.178934, -0.212211, 0.155545, -0.131057, -0.054052, 0.026799, -0.104970, -0.038684, -0.049633, -0.010591, -0.002685, 0.012570, -0.096096, -0.030982, -0.036971, -0.042177, -0.071497, -0.123253, 0.000922, -0.055256, 0.002017, -0.045009 },
        { 0.296652, 0.013720, 0.083646, 0.027470, 0.142282, 0.068142, 0.044859, 0.000680, 0.139084, -0.166206, 0.050939, -0.004675, 0.032548, 0.035593, -0.007277, 0.041595, -0.005147, 0.079604, 0.013484, 0.033600, 0.034853, 0.116119, 0.037021, -0.035234, -0.007265, 0.059269, -0.000984, 0.037632, 0.011773, 0.061140, 0.012118, 0.014515, -0.022180, 0.035422, -0.016126, 0.101324, 0.021144, -0.033725, 0.035089, -0.002996, 0.033160, -0.021882 },
        { -0.072607, -0.000264, -0.066374, 0.032500, -0.061581, 0.003436, -0.094821, -0.077710, -0.021220, -0.034062, -0.006930, -0.006373, 0.001249, 0.002187, -0.018181, 0.018922, -0.032497, 0.014611, 0.015621, 0.009928, -0.048406, -0.015910, -0.060203, -0.027370, -0.026799, 0.039281, -0.068357, 0.060067, -0.095069, 0.077196, -0.007151, 0.017255, -0.014574, 0.054118, -0.105354, 0.023937, -0.007243, 0.049627, 0.005831, 0.036551, -0.052230, 0.012507 },
        { -0.150168, 0.059990, -0.126519, -0.055997, -0.129019, -0.081577, -0.100124, -0.069299, -0.059040, -0.003220, -0.015029, -0.004946, -0.024989, -0.056588, 0.047901, -0.120403, 0.036527, -0.165062, 0.057440, -0.130219, 0.060142, -0.163759, 0.028285, 0.001099, -0.024681, -0.106922, 0.007998, -0.070893, -0.021552, -0.061452, -0.017591, -0.012435, 0.026575, -0.096739, -0.010642, -0.097875, 0.016664, -0.110713, 0.006331, -0.004551, -0.029567, 0.000151 },
        { 0.182877, -0.011617, 0.136802, 0.008815, 0.114402, -0.007940, 0.249886, -0.148061, 0.268219, -0.262000, 0.016682, 0.002775, -0.003441, -0.004498, -0.018288, -0.005000, -0.057552, -0.004462, -0.026300, -0.009436, -0.020572, -0.038174, 0.042075, -0.011029, -0.001992, -0.001820, 0.046571, -0.030358, 0.007682, -0.030230, -0.020251, -0.000302, 0.064942, 0.069263, 0.017017, 0.035670, -0.195102, -0.184862, -0.002260, -0.045087, -0.024763, -0.015997 },
        { -0.133559, 0.018382, -0.073912, 0.005526, 0.019276, 0.091564, -0.151146, -0.042698, -0.023180, 0.024285, 0.004274, 0.022792, 0.045099, 0.046466, 0.030734, -0.003764, 0.094301, 0.045582, -0.024339, -0.085029, -0.009309, 0.091068, -0.010012, 0.030136, -0.058016, 0.033237, -0.073995, -0.042575, -0.092633, 0.051267, 0.023585, 0.041981, -0.119711, -0.037480, -0.083207, 0.073374, 0.006771, 0.110230, 0.045905, -0.022500, 0.043258, -0.008800 }
        } };

array<array<double, 6>, 3> INIT_BIAS = { {
        { 0.156375, 0.651140, 0.375675, -0.291848, -0.377541, 0.410603 },
        { 0.678611, 0.023789, -1.081473, 0.470712, 0.982475, 0.417782 },
        { 1.385717, 1.242863, 1.202358, 0.806627, 0.420375, 0.185702 }
        } };

array<array<array<double, 6>, 6>, 3> INIT_LAYERS = { {
        {{ // layer 1
                { -0.080481, 0.224752, -0.628320, -0.777262, -0.450444, 0.379071 },
                { -1.247337, -0.280470, -0.187675, -0.272864, -0.194746, -0.886473 },
                { -0.459884, -0.298271, -0.739753, -0.734399, -0.872408, 0.268788 },
                { 0.424629, 0.073495, 0.280565, 0.861794, 0.656289, -0.235575 },
                { 0.653003, -0.234481, -0.004438, 0.124283, 0.178723, 0.655430 },
                { -0.416834, 0.020532, -0.422632, -0.767066, -0.552167, 0.139317 }
        }},
        {{ // layer 2
                { 0.241928, 1.430317, -0.983906, -0.350342, -1.633247, 0.279819 },
                { 0.882303, -0.051972, 0.105404, -0.904592, -0.515778, 0.520896 },
                { -0.352268, -1.224383, -0.705030, 0.633352, 0.686623, -0.673150 },
                { 0.677745, 0.193210, 0.692699, -1.065570, -0.638250, 1.042088 },
                { 0.229172, 1.023350, 0.550095, -0.677661, -0.507384, 0.554966 },
                { 0.850877, 0.249267, 0.954459, -0.826071, -0.584172, 0.992748 }
        }},
        {{ // layer 3
                { 0.082429, 0.170759, -0.009657, 0.045538, 0.116927, 0.004838 },
                { 0.222501, 0.157778, -0.098414, 0.096449, 0.173526, 0.072392 },
                { -0.142569, -0.073291, 0.349858, -0.201169, -0.203992, -0.283566 },
                { 0.022968, 0.157492, -0.117309, 0.097221, 0.211581, 0.175003 },
                { 0.246553, -0.085495, 0.132638, -0.142777, -0.077494, -0.203869 },
                { 0.022523, 0.065907, 0.006411, 0.061306, 0.004581, -0.015592 }
        }}
        } };
}
namespace NNMul
{
array<array<double, 6>, 5> INIT_ODD = NNMat::INIT_ODD;

array<array<double, 42>, 6> INIT_EVEN = { {
        { 0.309503, 0.322706, 0.207529, 0.032179, 0.144495, 0.024223, 0.152015, 0.105946, 0.286003, -0.285329, 0.110691, -0.057631, 0.187635, -0.163412, 0.222465, 0.253213, 0.214156, 0.186025, 0.193442, 0.277107, 0.241804, 0.261959, 0.153176, 0.139938, 0.110589, 0.046510, 0.071430, 0.113916, 0.157318, 0.083062, 0.038131, 0.017244, 0.078333, 0.054200, 0.172989, 0.034588, 0.099059, 0.095571, 0.052731, 0.044025, 0.068490, 0.009975 },
        { -0.382258, -0.628318, -0.193927, 1.291475, -0.136403, -0.006459, -0.143683, 0.328044, -0.155674, -1.171912, -0.081645, 0.010424, -0.049251, 0.042793, -2.469888, 1.219448, -0.657358, -0.225497, -1.555532, 0.509997, -0.303631, -0.197270, -0.356156, -0.162578, -0.342951, -0.000231, -1.425357, 1.078373, -0.906510, 0.556250, -0.206696, -0.032035, -0.320991, 0.059271, 0.013095, -0.204018, -0.348001, 0.120669, -0.196435, -0.008252, -0.003457, -0.114759 },
        { -0.206703, -0.198595, -0.203482, 0.025691, -0.206446, 0.039166, -0.183688, 0.159039, -0.271361, 0.026219, 0.056517, -0.098774, 0.364370, -0.325204, -0.413980, -0.085245, -0.207183, -0.227676, -0.372344, -0.124552, -0.207242, -0.214479, -0.031915, -0.048707, 0.225458, -0.336476, -0.247807, 0.135044, 0.750238, -0.859692, 0.132778, -0.092017, 0.068439, -0.204284, -0.164798, 0.048560, 0.348715, -0.446517, 0.075735, -0.036780, 0.013371, 0.068087 },
        { -0.208659, -0.277220, -0.116913, 0.049283, -0.108449, -0.012259, -0.105457, -0.013578, -0.120389, 0.033765, -0.011950, 0.001777, 0.018158, -0.030417, 0.115973, -0.352481, -0.062924, -0.068499, 0.232211, -0.318274, 0.291016, -0.296568, 0.059516, 0.055686, -0.052740, -0.052952, -0.095168, 0.009601, 0.043714, -0.175937, 0.006322, 0.006707, -0.064619, -0.037871, -0.083851, -0.052865, 0.021071, -0.152243, -0.003029, 0.011815, -0.008697, 0.006260 },
        { 0.069683, -0.804664, -0.136120, -0.300403, -0.130396, -0.030022, -0.139873, -0.048006, -0.176769, 0.593151, 0.003744, -0.025770, -0.012546, -0.017396, 1.216853, -0.792999, 0.250908, 0.051397, 0.846585, -0.510427, 0.458545, -0.218125, 0.044835, -0.026732, -0.010117, -0.064790, 0.067831, -0.181335, 0.159078, -0.418780, 0.013029, -0.006890, -0.057680, -0.066836, -0.202729, -0.094385, -0.030647, -0.268648, -0.008362, -0.010523, -0.063029, -0.018978 },
        { -0.578549, 0.936030, 0.005930, -0.770917, 0.014755, 0.000635, -0.011590, -0.072430, 0.044733, 0.433542, -0.014780, 0.000094, -0.165143, 0.165778, -1.050373, 0.735154, -0.165916, 0.008798, -0.705070, 0.412396, -0.320753, -0.006742, -0.171485, -0.077346, 0.084230, 0.094788, 0.706201, -0.538772, 0.068337, 0.109349, 0.058665, 0.057531, 0.134506, 0.024387, 0.072174, 0.088411, -0.058692, 0.200677, 0.050063, 0.001424, -0.002402, 0.020859 }
        } };

array<array<double, 6>, 5> INIT_BIAS = { {
        { 0.101635, 0.297808, -0.238580, 0.252065, -0.125309, -0.253675 },
        { -0.717912, 0.395154, 0.323444, -0.318099, 0.566692, -0.547057 },
        { -1.009594, 0.475792, -0.927119, 0.876152, -0.403080, -0.773306 },
        { 0.825231, -0.626942, -0.759996, -0.709031, -0.301317, -0.448507 },
        { -0.044214, -0.279977, 0.103766, 0.013387, 0.478699, -0.553257 }
        } };

array<array<array<double, 6>, 6>, 5> INIT_LAYERS = { {
        {{ // layer 1
                { -0.035486, -1.261542, -0.360403, -0.007991, -0.012490, -0.119738 },
                { 0.276694, -0.286199, 0.733428, -0.159535, -0.181264, 0.550596 },
                { -0.248973, 0.319819, -0.626034, 0.122913, 0.058356, -0.664660 },
                { 0.342338, -0.142871, 0.766051, -0.305705, -0.448469, 0.302739 },
                { -0.063314, 0.904643, -0.073224, 0.009866, -0.086836, -0.492848 },
                { -0.285748, 0.216442, -0.731083, 0.213010, 0.323802, -0.436208 }
        }},
        {{ // layer 2
                { -0.372555, -0.547843, 0.607317, -0.061417, 0.798082, 0.393816 },
                { 1.273244, 0.263865, -0.284210, -0.025524, -0.919909, -0.153988 },
                { -0.323625, 0.763050, -0.747422, 0.960095, -0.378402, -0.946870 },
                { 0.302958, -0.740946, 0.764207, -0.836658, 0.365923, 0.873464 },
                { 0.151462, 0.582104, -0.657181, 0.286329, -0.602582, -0.487701 },
                { -0.156704, -0.567882, 0.638627, -0.286208, 0.596313, 0.477499 }
        }},
        {{ // layer 3
                { 0.861211, -0.426188, -0.711483, 0.809920, -0.926319, 0.918573 },
                { -0.404106, -0.279742, 2.240501, -1.920244, 0.692765, -0.673043 },
                { 0.809263, -0.407157, -0.687232, 0.764543, -0.846475, 0.842688 },
                { -0.778212, 0.390023, 0.689402, -0.757262, 0.822226, -0.817103 },
                { 0.538498, -0.941191, -0.396048, 0.479170, -0.519340, 0.518210 },
                { 0.721263, -0.360799, -0.640420, 0.698920, -0.748171, 0.743517 }
        }},
        {{ // layer 4
                { -0.822751, 0.503417, -0.823588, 0.813567, -0.755963, -0.791489 },
                { 0.597349, -0.982341, 0.598485, -0.587777, 0.572002, 0.570639 },
                { 0.758645, -1.444528, 0.743339, -0.750135, 0.702050, 0.755309 },
                { 0.699194, -0.457681, 0.698962, -0.691376, 0.678990, 0.685872 },
                { 0.451486, 0.299530, 0.451330, -0.440794, 0.423834, 0.463594 },
                { 0.428851, -0.705959, 0.431486, -0.428847, 0.411780, 0.441115 }
        }},
        {{ // layer 5
                { 0.140251, 0.027183, 0.043495, -0.042418, -0.078724, 0.154763 },
                { -0.195654, 0.192687, 0.163403, -0.477620, -0.522896, 0.185687 },
                { -0.104522, -0.131798, -0.008283, -0.028928, 0.039757, 0.125282 },
                { 0.543815, 0.260477, 0.088270, 0.350081, -0.186406, 0.052956 },
                { -0.360391, -0.279877, -0.179441, 0.032982, 0.343650, 0.209640 },
                { 0.416068, 0.007320, -0.369529, -0.026143, 0.222796, -0.544806 }
        }}
        } };
}

template<int N_LAYERS>
struct MatModel_
{
	array<array<double, 6>, 5> odd1_;  // scoring weights
	array<array<double, 42>, 6> even1_;	// features for neural net
	array<array<array<double, 6>, 6>, N_LAYERS> layers_;  // N_LAYERS layers of 6 neurons each, fully connected
	array<array<double, 6>, N_LAYERS> biases_;  // biases for the N_LAYERS layers
	array<double, 6> oddVols_;

	double score(int mat_index, const double* target, double tweak, double* debug)
	{
		constexpr double SCORE_NORM = 247.0;

		int i = mat_index;
		int n_wq = i % 3;
		i /= 3;
		int n_bq = i % 3;
		i /= 3;
		int n_wr = i % 3;
		i /= 3;
		int n_br = i % 3;
		i /= 3;
		int n_wl = i % 2;
		i /= 2;
		int n_bl = i % 2;
		i /= 2;
		int n_wd = i % 2;
		i /= 2;
		int n_bd = i % 2;
		i /= 2;
		int n_wn = i % 3;
		i /= 3;
		int n_bn = i % 3;
		i /= 3;
		int n_wp = i % 9;
		i /= 9;
		int n_bp = i % 9;

		// collect material into features
		int n_wb = n_wl + n_wd, n_bb = n_bl + n_bd;
		int n_wbp = n_wl * n_wd, n_bbp = n_bl * n_bd;

		array<int, 6> oddFeatures = { n_wp - n_bp, n_wn - n_bn, n_wb - n_bb, n_wr - n_br, n_wq - n_bq, n_wbp - n_bbp };
		array<double, 5> odd1;
		for (int io = 0; io < 5; ++io)
			odd1[io] = inner_product(oddFeatures.begin(), oddFeatures.end(), odd1_[io].begin(), 0.0);

		array<int, 7> whiteFeatures = { n_wp, n_wn, n_wb, n_wr, n_wq, n_wbp };
		array<int, 7> blackFeatures = { n_bp, n_bn, n_bb, n_br, n_bq, n_bbp };

		auto fSum = [&](int ii) { return whiteFeatures[ii] + blackFeatures[ii]; };
		auto fProd = [&](int ii) { return whiteFeatures[ii] * blackFeatures[ii]; };
		auto qSame = [&](int ii, int jj) { return whiteFeatures[ii] * whiteFeatures[jj] + blackFeatures[ii] * blackFeatures[jj]; };
		auto qCross = [&](int ii, int jj) { return whiteFeatures[ii] * blackFeatures[jj] + blackFeatures[ii] * whiteFeatures[jj]; };

		array<int, 42> evenFeatures = { fSum(0), fProd(0), fSum(1), fProd(1), fSum(2), fProd(2), fSum(3), fProd(3), fSum(4), fProd(4), fSum(5), fProd(5), n_wl * n_bl + n_wd * n_bd, n_wl * n_bd + n_bl * n_wd,
				qSame(0, 1), qCross(0, 1), qSame(0, 2), qCross(0, 2), qSame(0, 3), qCross(0, 3), qSame(0, 4), qCross(0, 4), qSame(0, 5), qCross(0, 5), qSame(1, 2), qCross(1, 2), qSame(1, 3), qCross(1, 3), 
				qSame(1, 4), qCross(1, 4), qSame(1, 5), qCross(1, 5), qSame(2, 3), qCross(2, 3), qSame(2, 4), qCross(2, 4), qSame(3, 4), qCross(3, 4), qSame(3, 5), qCross(3, 5), qSame(4, 5), qCross(4, 5) };
		array<array<double, 6>, N_LAYERS + 1> activations;
		for (int ie = 0; ie < 6; ++ie)
			activations[0][ie] = tanh(inner_product(evenFeatures.begin(), evenFeatures.end(), even1_[ie].begin(), 0.0));

		for (int il = 0; il < N_LAYERS; ++il)
		{
			for (int ii = 0; ii < 6; ++ii)
			{
				activations[il + 1][ii] = inner_product(activations[il].begin(), activations[il].end(), layers_[il][ii].begin(), biases_[il][ii]);
				activations[il + 1][ii] = tanh(activations[il + 1][ii]);
			}
		}

		const auto& back = activations.back();

		double mul = (1.0 + back.back()) / 2.0;
		double score = min(1.0, mul) * inner_product(odd1.begin(), odd1.end(), back.begin(), 0.0);

		if (target && tweak && *target != 0.0)
		{
			double epsilon = 0.0010 * tweak;
			double delta = tweak * min(1.0, mul) * (*target - score) / SCORE_NORM;
			double deltaM = mul > 0.0 && mul < 1.0 ? (delta * score > 0.0 ? 1.0 : -1.0) * delta : 0.0;
			array<double, 6> reward, pvsReward;
			for (int ii = 0; ii < 5; ++ii)
				reward[ii] = delta * (odd1[ii] / SCORE_NORM) * (1.0 - Square(back[ii]));
			reward[5] = deltaM * (fabs(score) / SCORE_NORM) * (1.0 - Square(back.back()));
			//cout << "Reward: " << reward[5] << ", " << mul << ", " << back[5] << ", " << deltaM << ", " << targets->second << ", " << reward[5] << "\n";

			// work backward through the layers
			for (int il = N_LAYERS - 1; il >= 0; --il)
			{
				double rNorm = 0.0;
				for (int ii = 0; ii < 6; ++ii)
				{
					biases_[il][ii] += reward[ii] - epsilon * (10 + ii) * biases_[il][ii];

					pvsReward[ii] = 0.0;
					for (int jj = 0; jj < 6; ++jj)
					{
						pvsReward[ii] += layers_[il][jj][ii] * reward[jj];
						layers_[il][jj][ii] += activations[il][ii] * reward[jj] - epsilon * (10 + jj) * layers_[il][jj][ii];
					}
					pvsReward[ii] *= 1.0 - Square(activations[il][ii]);
					rNorm += Square(pvsReward[ii]);
				}
				reward = pvsReward;
				if (rNorm > 0.0)
				{
					double scale = tweak / sqrt(rNorm);
					for (int ii = 0; ii < 6; ++ii)
						reward[ii] *= scale;
				}
			}
			for (int ii = 0; ii < 42; ++ii)
				for (int jj = 0; jj < 6; ++jj)
					even1_[jj][ii] += evenFeatures[ii] * reward[jj] - epsilon * (10 + jj) * even1_[jj][ii];

			// also update the odd weights
			for (int iw = 0; iw < 5; ++iw)
				for (int ii = 0; ii < 6; ++ii)
					odd1_[iw][ii] += activations.back()[iw] * delta * oddVols_[ii] * oddFeatures[ii] - epsilon * (10 + iw) * odd1_[iw][ii];
		}
		if (debug)
		    *debug = activations[0][5];
		return score;
	}

	pair<double, double> muls(int mat_index, const pair<double, double>* target, double my_score, double tweak)
	{
		constexpr double SCORE_NORM = 150.0;

		int i = mat_index;
		int n_wq = i % 3;
		i /= 3;
		int n_bq = i % 3;
		i /= 3;
		int n_wr = i % 3;
		i /= 3;
		int n_br = i % 3;
		i /= 3;
		int n_wl = i % 2;
		i /= 2;
		int n_bl = i % 2;
		i /= 2;
		int n_wd = i % 2;
		i /= 2;
		int n_bd = i % 2;
		i /= 2;
		int n_wn = i % 3;
		i /= 3;
		int n_bn = i % 3;
		i /= 3;
		int n_wp = i % 9;
		i /= 9;
		int n_bp = i % 9;

		// collect material into features
		int n_wb = n_wl + n_wd, n_bb = n_bl + n_bd;
		int n_wbp = n_wl * n_wd, n_bbp = n_bl * n_bd;

		array<int, 6> oddFeatures = { n_wp - n_bp, n_wn - n_bn, n_wb - n_bb, n_wr - n_br, n_wq - n_bq, n_wbp - n_bbp };
		array<double, 5> odd1;
		for (int io = 0; io < 5; ++io)
			odd1[io] = inner_product(oddFeatures.begin(), oddFeatures.end(), odd1_[io].begin(), 0.0);

		array<int, 7> whiteFeatures = { n_wp, n_wn, n_wb, n_wr, n_wq, n_wbp };
		array<int, 7> blackFeatures = { n_bp, n_bn, n_bb, n_br, n_bq, n_bbp };

		auto fSum = [&](int ii) { return whiteFeatures[ii] + blackFeatures[ii]; };
		auto fProd = [&](int ii) { return whiteFeatures[ii] * blackFeatures[ii]; };
		auto qSame = [&](int ii, int jj) { return whiteFeatures[ii] * whiteFeatures[jj] + blackFeatures[ii] * blackFeatures[jj]; };
		auto qCross = [&](int ii, int jj) { return whiteFeatures[ii] * blackFeatures[jj] + blackFeatures[ii] * whiteFeatures[jj]; };

		array<int, 42> evenFeatures = { fSum(0), fProd(0), fSum(1), fProd(1), fSum(2), fProd(2), fSum(3), fProd(3), fSum(4), fProd(4), fSum(5), fProd(5), n_wl * n_bl + n_wd * n_bd, n_wl * n_bd + n_bl * n_wd,
				qSame(0, 1), qCross(0, 1), qSame(0, 2), qCross(0, 2), qSame(0, 3), qCross(0, 3), qSame(0, 4), qCross(0, 4), qSame(0, 5), qCross(0, 5), qSame(1, 2), qCross(1, 2), qSame(1, 3), qCross(1, 3), 
				qSame(1, 4), qCross(1, 4), qSame(1, 5), qCross(1, 5), qSame(2, 3), qCross(2, 3), qSame(2, 4), qCross(2, 4), qSame(3, 4), qCross(3, 4), qSame(3, 5), qCross(3, 5), qSame(4, 5), qCross(4, 5) };
		array<array<double, 6>, N_LAYERS + 1> activations;
		for (int ie = 0; ie < 6; ++ie)
			activations[0][ie] = tanh(inner_product(evenFeatures.begin(), evenFeatures.end(), even1_[ie].begin(), 0.0));

		for (int il = 0; il < N_LAYERS; ++il)
		{
			for (int ii = 0; ii < 6; ++ii)
			{
				activations[il + 1][ii] = inner_product(activations[il].begin(), activations[il].end(), layers_[il][ii].begin(), biases_[il][ii]);
				activations[il + 1][ii] = tanh(activations[il + 1][ii]);
			}
		}

		const array<double, 6>& back = activations.back();
		array<int, 6> wAdvantage = { n_wp - n_bp, n_wn - n_bn, n_wb - n_bb, n_wr - n_br, n_wq - n_bq, 1 };
		double wDot = inner_product(wAdvantage.begin(), wAdvantage.end(), back.begin(), 0.0), bDot = 2.0 * back.back() - wDot;

		double wMul = max(0.0, 0.5 + tanh(wDot)), bMul = max(0.0, 0.5 + tanh(bDot));
		if (target && tweak && target->first != 0.0 && target->second != 0.0)
		{
			double epsilon = 0.0;
			double wWeight = (1.0 + tanh(my_score / SCORE_NORM)) / 2.0;
			double deltaW = tweak * wWeight * (target->first - wMul);
			double deltaB = tweak * (1.0 - wWeight) * (target->second - bMul);
			array<double, 6> reward, pvsReward;
			for (int ii = 0; ii < 5; ++ii)
				reward[ii] = (deltaW - deltaB) * wAdvantage[ii] * (1.0 - Square(back[ii]));
			reward.back() = (deltaW + deltaB) * (1.0 - Square(back.back()));

			// work backward through the layers
			for (int il = N_LAYERS - 1; il >= 0; --il)
			{
				double rNorm = 0.0;
				for (int ii = 0; ii < 6; ++ii)
				{
					biases_[il][ii] += reward[ii] - epsilon * (10 + ii) * biases_[il][ii];

					pvsReward[ii] = 0.0;
					for (int jj = 0; jj < 6; ++jj)
					{
						pvsReward[ii] += layers_[il][jj][ii] * reward[jj];
						layers_[il][jj][ii] += activations[il][ii] * reward[jj] - epsilon * (10 + jj) * layers_[il][jj][ii];
					}
					pvsReward[ii] *= 1.0 - Square(activations[il][ii]);
					rNorm += Square(pvsReward[ii]);
				}
				reward = pvsReward;
				if (rNorm > 0.0)
				{
					double scale = tweak / sqrt(rNorm);
					for (int ii = 0; ii < 6; ++ii)
						reward[ii] *= scale;
				}
     			epsilon = 0.0010 * tweak;
			}
			for (int ii = 0; ii < 42; ++ii)
				for (int jj = 0; jj < 6; ++jj)
					even1_[jj][ii] += evenFeatures[ii] * reward[jj] - epsilon * (10 + jj) * even1_[jj][ii];
		}
		return make_pair(wMul, bMul);
	}
};
