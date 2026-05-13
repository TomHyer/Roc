#include <iostream>
#include <memory>
#include <numeric>
#include <cmath>

using namespace std;

array<array<double, 6>, 5> INIT_ODD = { {
	{386.131, 918.782, 796.06, 1556.82, 2766.46, 100.262},
	{200.901, 790.212, 797.696, 1169.91, 2263.84, 90.121},
	{-140.035, -657.506, -675.42, -953.866, -1873.44, -75.3869},
	{-1.0, 1.0, 1.0, -1.0, 1.0, 1.0},
	{-1.0, 1.0, 1.0, 1.0, 1.0, -1.0}
	} };

array<double, 6> ODD_VOLS = { 10000.0, 30000.0, 30000.0, 50000.0, 90000.0, 10000.0 };

array<array<double, 42>, 6> INIT_EVEN = { {
	{ -1.33678, -0.0840098, -0.422913, -0.129761, -0.458867, -0.00645146, -0.180468, -0.12156, -0.0686781, 0.848829, -0.0474653, 0.0161362, 0.15156, -0.158011, 0.188356, -0.327029, 0.213997, -0.651508, 0.47369, 0.411427, 1.34417, 0.130955, 0.305088, -0.0107152, -0.206454, -0.111265, 0.127395, 0.124927, 0.501463, 0.207588, -0.00389197, 0.0115113, 0.310952, 0.131701, 0.788291, 0.18803, 0.31437, 0.371801, 0.226014, 0.0847463, 0.414451, 0.136145 },
	{ -1.28755, -0.25464, -0.461592, -0.0070317, -0.40122, 0.078873, -0.490876, -0.102016, -0.191447, 0.627043, 0.0620199, 0.00497044, 0.29902, -0.220147, 0.251114, -0.357921, 0.408628, -0.540647, 0.939068, -0.530704, 2.25111, -0.409701, 0.339244, 0.353467, -0.148015, 0.0964876, -0.0646659, 0.129346, 0.0981471, 0.42869, 0.138617, 0.103141, -0.116888, 0.271985, 0.0256659, 0.759599, 0.0453656, 0.0760577, 0.1669, 0.302152, 0.229824, 0.372725 },
	{ -0.239813, -0.0600654, -0.0650848, -0.0362255, -0.0719042, -0.0553283, -0.0940443, -0.0338986, -0.144092, -0.122531, -0.0151477, -0.00551376, -0.0343914, -0.0209369, -0.139645, -0.146336, -0.170524, -0.160953, -0.28168, -0.171252, -0.441089, -0.29718, -0.0182685, -0.0286025, -0.0137499, -0.0920388, -0.0761905, -0.0536006, -0.0993132, -0.122206, 0.00737832, -0.0326168, -0.075692, -0.0748542, -0.117052, -0.133188, -0.17783, -0.152655, -0.0110593, -0.0161686, -0.023681, -0.0328135 },
	{ -0.0429753, 0.16866, 0.0165276, 0.0304547, 0.035752, 0.046321, -0.0608001, -0.145617, -0.24277, -0.486691, -0.0226827, 0.0010609, -0.0150096, 0.0613306, -0.106284, 0.0596121, 0.0622673, 0.191905, -0.31429, -0.544349, -0.854311, 0.101038, -0.13774, 0.0791547, 0.0627451, 0.148209, -0.0434291, -0.0668715, -0.196307, -0.193919, 0.0104885, 0.0292017, -0.166913, 0.0198057, -0.345803, -0.225652, -0.174785, -0.338481, -0.0562214, -0.0276906, -0.115893, -0.109318 },
	{ -0.236513, -0.0617033, -0.0662417, -0.0321646, -0.0735445, -0.049313, -0.0980448, -0.0369403, -0.145938, -0.120538, -0.0168501, -0.00450352, -0.0306265, -0.0186865, -0.14815, -0.136767, -0.179966, -0.150391, -0.280416, -0.174695, -0.436744, -0.290053, -0.0286383, -0.0264482, -0.0244137, -0.0830343, -0.0761003, -0.059432, -0.103875, -0.116083, 0.000702397, -0.0282433, -0.0776066, -0.0805188, -0.122296, -0.125686, -0.176026, -0.156306, -0.0135629, -0.0194843, -0.0280955, -0.0303803 },
	{ 0.235804, 0.0617123, 0.0661583, 0.0316833, 0.0736582, 0.0486657, 0.0979964, 0.0371265, 0.146382, 0.120777, 0.0168539, 0.00440032, 0.0300841, 0.0185816, 0.148349, 0.135738, 0.180074, 0.149201, 0.279937, 0.174671, 0.435883, 0.288865, 0.0290124, 0.0261765, 0.0254549, 0.0817752, 0.0755294, 0.0596649, 0.104149, 0.115682, -0.000185604, 0.027609, 0.0774855, 0.0806977, 0.122683, 0.125671, 0.175229, 0.156189, 0.0136169, 0.0195846, 0.0281617, 0.0303327 }
	} };

array<array<double, 6>, 3> INIT_BIAS = { {
	{ 0.227965, 0.668725, 0.660269, 0.65254, -0.645431, -0.297013 },
	{ -0.311533, -0.318234, 0.32409, -0.329216, 0.531423, 0.337723 },
	{ 0.752173, 0.375867, -0.34429, 0.0, 0.0, 0.103658 }
	} };

array<array<array<double, 6>, 6>, 3> INIT_LAYERS = { {
	{{ // layer 1
		{ 1.92676, 0.979954, -0.230332, -0.318469, -0.230345, 0.23035 },
   { 0.537296, 0.553765, -0.66911, -0.529883, -0.669109, 0.669109 },
    { 0.535607, 0.551789, -0.660633, -0.52806, -0.660632, 0.660632 },
    { 0.533999, 0.549914, -0.652886, -0.526332, -0.652885, 0.652885 },
    { -0.532459, -0.548126, 0.645761, 0.524684, 0.64576, -0.64576 },
    { -0.606231, -1.78411, 0.298261, 0.343371, 0.29822, -0.29822 }
	}},
	{{ // layer 2
		{ -0.0796501, -0.295816, -0.294987, -0.294203, 0.293459, 0.317279 },
    { -0.112369, -0.304059, -0.303309, -0.302599, 0.301925, 0.299447 },
    { 0.138928, 0.311156, 0.31047, 0.30982, -0.309203, -0.289079 },
    {-0.161032, -0.317304, -0.31667, -0.31607, 0.3155, 0.283402 },
    { 1.05597, 0.528212, 0.528131, 0.528064, -0.52801, -0.74838 },
    { 0.196024, 0.327406, 0.326855, 0.326333, -0.325837, -0.279978 }
	}},
	{{ // layer 3
		{ 0.0509744, 0.0515158, -0.0519559, 0.0523298, -0.424273, -0.0529678 },
   { -0.223629, -0.225544, 0.226894, -0.227852, 0.1422, 0.229021 },
    { 0.195057, 0.196656, -0.19778, 0.19857, -0.112786, -0.199513 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 },
    { -0.0819857, -0.0827412, 0.0832892, -0.083693, 0.0720765, 0.0842248 }
	}}
} };

struct MatModel_
{
	array<array<double, 6>, 5> odd1_;  // scoring weights
	array<array<double, 42>, 6> even1_;	// features for neural net
	array<array<array<double, 6>, 6>, 3> layers_;  // 3 layers of 6 neurons each, fully connected
	array<array<double, 6>, 3> biases_;  // biases for the 3 layers
	array<double, 6> oddVols_;

	pair<double, double> score(int mat_index, const pair<double, double>* targets, double tweak, double* debug)
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
		array<array<double, 6>, 4> activations;
		for (int ie = 0; ie < 6; ++ie)
			activations[0][ie] = tanh(inner_product(evenFeatures.begin(), evenFeatures.end(), even1_[ie].begin(), 0.0));

		for (int il = 0; il < 3; ++il)
		{
			for (int ii = 0; ii < 6; ++ii)
			{
				activations[il + 1][ii] = inner_product(activations[il].begin(), activations[il].end(), layers_[il][ii].begin(), biases_[il][ii]);
				activations[il + 1][ii] = tanh(activations[il + 1][ii]);
			}
		}

		double mul = max(0.0, 0.5 + tanh(activations[3].back()));
		double score = min(1.0, mul) * inner_product(odd1.begin(), odd1.end(), activations[3].begin(), 0.0);

		if (targets && tweak)
		{
			double epsilon = 0.0050 * tweak;
			double delta = tweak * min(1.0, mul) * (targets->first - score) / SCORE_NORM;
			double deltaM = tweak * (targets->second - mul);
			if (mul < 1.0)
			{
				deltaM += (delta > 0.0 ? 1.0 : -1.0) * 0.1 * min(abs(delta), abs(deltaM));
				delta *= mul;
			}
			array<double, 6> reward, pvsReward;
			for (int ii = 0; ii < 5; ++ii)
				reward[ii] = delta * (odd1[ii] / SCORE_NORM) * (1.0 - Square(activations[3][ii]));
			reward[5] = deltaM * (1.0 - Square(activations[3].back()));
			//cout << "Reward: " << reward[5] << ", " << mul << ", " << activations[3][5] << ", " << deltaM << ", " << targets->second << ", " << reward[5] << "\n";

			// work backward through the layers
			for (int il = 2; il >= 0; --il)
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
		return make_pair(score, mul);
	}

};
