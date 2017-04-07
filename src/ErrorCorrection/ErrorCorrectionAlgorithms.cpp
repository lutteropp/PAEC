#include <stddef.h>
#include <cassert>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#include "../CorrectedRead.h"
#include "../ErrorProfile/ErrorProfileUnit.hpp"
#include "../ErrorType.h"
#include "../FASTQRead.h"
#include "../KmerClassification/KmerClassificationUnit.h"
#include "../KmerClassification/KmerType.h"

std::pair<ErrorType, double> mostLikelyCurrentBase(std::unordered_map<ErrorType, double> &errorProbabilities) {
	double bestProb = errorProbabilities[ErrorType::CORRECT];
	ErrorType bestType = ErrorType::CORRECT;
	for (ErrorType type : errorTypesCurrentBase()) {
		if (errorProbabilities[type] > bestProb) {
			bestProb = errorProbabilities[type];
			bestType = type;
		}
	}
	return std::make_pair(bestType, bestProb);
}

std::pair<ErrorType, double> mostLikelyNextGap(std::unordered_map<ErrorType, double> &errorProbabilities) {
	double bestProb = errorProbabilities[ErrorType::NODEL];
	ErrorType bestType = ErrorType::NODEL;
	for (ErrorType type : errorTypesNextGap()) {
		if (errorProbabilities[type] > bestProb) {
			bestProb = errorProbabilities[type];
			bestType = type;
		}
	}
	return std::make_pair(bestType, bestProb);
}

std::string kmerAfterError(const std::string &kmer, ErrorType error, int posOfError) {
	std::string res = kmer;
	if (error == ErrorType::SUB_FROM_A) {
		res[posOfError] = 'A';
	} else if (error == ErrorType::SUB_FROM_C) {
		res[posOfError] = 'C';
	} else if (error == ErrorType::SUB_FROM_G) {
		res[posOfError] = 'G';
	} else if (error == ErrorType::SUB_FROM_T) {
		res[posOfError] = 'T';
	} else if (error == ErrorType::INSERTION) {
		res = kmer.substr(0, posOfError) + kmer.substr(posOfError + 1, kmer.size());
	} else if (error == ErrorType::DEL_OF_A) {
		res = kmer.substr(0, posOfError + 1) + "A" + kmer.substr(posOfError + 1, kmer.size());
	} else if (error == ErrorType::DEL_OF_C) {
		res = kmer.substr(0, posOfError + 1) + "C" + kmer.substr(posOfError + 1, kmer.size());
	} else if (error == ErrorType::DEL_OF_G) {
		res = kmer.substr(0, posOfError + 1) + "G" + kmer.substr(posOfError + 1, kmer.size());
	} else if (error == ErrorType::DEL_OF_T) {
		res = kmer.substr(0, posOfError + 1) + "T" + kmer.substr(posOfError + 1, kmer.size());
	} else { // multideletion or chimeric break
		res = kmer.substr(0, posOfError + 1) + "_" + kmer.substr(posOfError + 1, kmer.size());
	}

	return res;
}

KmerType checkLeftOf(size_t pos, const CorrectedRead &corr, KmerClassificationUnit &kmerClassifier) {
	KmerType type = KmerType::REPEAT;
	if (pos >= kmerClassifier.getMinKmerSize()) {
		std::string kmerString = corr.correctedRead.sequence.substr(pos - kmerClassifier.getMinKmerSize(),
				kmerClassifier.getMinKmerSize());
		if (kmerString.find("_") != std::string::npos) {
			return type;
		}
		type = kmerClassifier.classifyKmer(kmerString);
		int i = pos - kmerClassifier.getMinKmerSize();
		while (type == KmerType::REPEAT && i - 2 >= 0) {
			if (corr.correctedRead.sequence[i - 1] == '_' || corr.correctedRead.sequence[i - 2] == '_') {
				break;
			}
			kmerString = corr.correctedRead.sequence[i - 1] + kmerString;
			kmerString = corr.correctedRead.sequence[i - 2] + kmerString;
			type = kmerClassifier.classifyKmer(kmerString);
			i -= 2;
		}
	}
	return type;
}

KmerType checkRightOf(size_t pos, const CorrectedRead &corr, KmerClassificationUnit &kmerClassifier) {
	KmerType type = KmerType::REPEAT;
	std::string kmerString = corr.correctedRead.sequence.substr(pos + 1, kmerClassifier.getMinKmerSize());
	if (kmerString.size() >= kmerClassifier.getMinKmerSize()) {
		if (kmerString.size() > kmerClassifier.getMinKmerSize()) {
			throw std::runtime_error("This should not happen,,");
		}

		std::string kmerString = corr.correctedRead.sequence.substr(pos + 1, kmerClassifier.getMinKmerSize());
		if (kmerString.find("_") != std::string::npos) {
			return type;
		}
		type = kmerClassifier.classifyKmer(kmerString);
		int i = pos + kmerClassifier.getMinKmerSize();
		while (type == KmerType::REPEAT && i + 2 < (int) corr.correctedRead.sequence.size()) {
			if (corr.correctedRead.sequence[i + 1] == '_' || corr.correctedRead.sequence[i + 2] == '_') {
				break;
			}
			kmerString = kmerString + corr.correctedRead.sequence[i + 1];
			kmerString = kmerString + corr.correctedRead.sequence[i + 2];
			type = kmerClassifier.classifyKmer(kmerString);
			i += 2;
		}
	}
	return type;
}

KmerType classifyMiddleWithPosAs(size_t posInRead, const std::string &sequence, KmerClassificationUnit &kmerClassifier,
		std::string middle = "") {
	bool leftPossible = (posInRead >= 1);
	bool rightPossible = (posInRead <= sequence.size() - 2);

	KmerType kmerType = KmerType::REPEAT;
	std::string kmer = middle;
	size_t offsetLeft = 1;
	size_t offsetRight = 1;
	bool goLeft = leftPossible;
	size_t maxKmerSize = sequence.size();

	while ((leftPossible || rightPossible) && kmer.size() < maxKmerSize) {
		if (goLeft) {
			if (offsetLeft > posInRead) {
				leftPossible = false;
			} else {
				if (sequence[posInRead - offsetLeft] == '_') {
					leftPossible = false;
				} else {
					kmer = sequence[posInRead - offsetLeft] + kmer;
					if (kmer.size() >= kmerClassifier.getMinKmerSize() && kmer.size() % 2 == 1) {
						KmerType type = kmerClassifier.classifyKmer(kmer);
						if (type != KmerType::REPEAT) {
							kmerType = type;
							break;
						}
					}
					offsetLeft++;
				}
			}
			if (rightPossible) {
				goLeft = false;
			}
		} else { // go right
			if (posInRead + offsetRight >= sequence.size()) {
				rightPossible = false;
			} else {
				if (sequence[posInRead + offsetRight] == '_') {
					rightPossible = false;
				} else {
					kmer = kmer + sequence[posInRead + offsetRight];
					if (kmer.size() >= kmerClassifier.getMinKmerSize() && kmer.size() % 2 == 1) {
						KmerType type = kmerClassifier.classifyKmer(kmer);
						if (type != KmerType::REPEAT) {
							kmerType = type;
							break;
						}
					}
					offsetRight++;
				}
			}
			if (leftPossible) {
				goLeft = true;
			}
		}
	}
	return kmerType;
}

//TODO: FIXME: this function is buggy
// used for multideletions, such as ab_cd, checks the type of abcd, i.e., without the gap in between.
KmerType classifyMiddleWithoutPos(size_t posInRead, const std::string &sequence,
		KmerClassificationUnit &kmerClassifier) {
	bool leftPossible = (posInRead >= 1);
	bool rightPossible = (posInRead <= sequence.size() - 2);

	KmerType kmerType = KmerType::REPEAT;
	std::string kmer = "";
	size_t offsetLeft = 1;
	size_t offsetRight = 1;
	bool goLeft = leftPossible;
	size_t maxKmerSize = sequence.size();

	while ((leftPossible || rightPossible) && kmer.size() < maxKmerSize) {
		if (goLeft) {
			if (offsetLeft > posInRead) {
				leftPossible = false;
			} else {
				if (sequence[posInRead - offsetLeft] == '_') {
					leftPossible = false;
				} else {
					kmer = sequence[posInRead - offsetLeft] + kmer;
					if (kmer.size() >= kmerClassifier.getMinKmerSize() && kmer.size() % 2 == 1) {
						KmerType type = kmerClassifier.classifyKmer(kmer);
						if (type != KmerType::REPEAT) {
							kmerType = type;
							break;
						}
					}
					offsetLeft++;
				}
			}
			if (rightPossible) {
				goLeft = false;
			}
		} else { // go right
			if (posInRead + offsetRight >= sequence.size()) {
				rightPossible = false;
			} else {
				if (sequence[posInRead + offsetRight] == '_') {
					rightPossible = false;
				} else {
					kmer = kmer + sequence[posInRead + offsetRight];
					if (kmer.size() >= kmerClassifier.getMinKmerSize() && kmer.size() % 2 == 1) {
						KmerType type = kmerClassifier.classifyKmer(kmer);
						if (type != KmerType::REPEAT) {
							kmerType = type;
							break;
						}
					}
					offsetRight++;
				}
			}
			if (leftPossible) {
				goLeft = true;
			}
		}
	}
	return kmerType;
}

bool resolveMultidel(CorrectedRead &corr, ErrorProfileUnit &errorProfile, KmerClassificationUnit &kmerClassifier,
		size_t multidelPos) {
	assert(corr.correctedRead.sequence[multidelPos] == '_');
	bool canExtendLeft = true;
	bool canExtendRight = true;
	bool doLeft = true;
	KmerType typeMiddle = classifyMiddleWithoutPos(multidelPos, corr.correctedRead.sequence, kmerClassifier);

	KmerType typeLeft = checkLeftOf(multidelPos, corr, kmerClassifier);
	KmerType typeRight = checkRightOf(multidelPos, corr, kmerClassifier);

	//std::string kmerLeft = corr.correctedRead.sequence.substr(multidelPos - kmerClassifier.getMinKmerSize(), kmerClassifier.getMinKmerSize());
	//std::cout << "Fixing multidel with kmerLeft = " << kmerLeft << "\n";

	if (typeLeft == KmerType::UNTRUSTED || typeRight == KmerType::UNTRUSTED) {
		throw std::runtime_error("Something is strange");
	}

	// if we only had (canExtendLeft || canExtendRight), we might end up in partial assembly!
	while ((canExtendLeft && canExtendRight) && typeMiddle != KmerType::TRUSTED) {
		if (multidelPos > corr.correctedRead.sequence.size()) {
			throw std::runtime_error("multidelPos > corr.correctedRead.sequence.size()");
		}
		// ... TODO: here comes the code
		if (doLeft) { // try left-extension
			std::string kmerLeft = corr.correctedRead.sequence.substr(
					std::max(0, (int) multidelPos - (int) kmerClassifier.getMinKmerSize()),
					kmerClassifier.getMinKmerSize());
			if (kmerLeft.find("_") != std::string::npos) {
				kmerLeft = kmerLeft.substr(0, kmerLeft.find("_"));
			}

			auto probs = errorProfile.getErrorProbabilities(corr.correctedRead, multidelPos - 1);
			// sort the possible corrections based on their probability
			std::vector<std::pair<ErrorType, double> > ranking;
			for (auto kv : probs) {
				if (kv.first == ErrorType::DEL_OF_A || kv.first == ErrorType::DEL_OF_C
						|| kv.first == ErrorType::DEL_OF_G || kv.first == ErrorType::DEL_OF_T) {
					ranking.push_back(kv);
				}
			}
			// sort ranking by descending kv.second
			std::sort(ranking.begin(), ranking.end(),
					[](const std::pair<ErrorType,double> &left, const std::pair<ErrorType,double> &right) {
						return left.second > right.second;
					});

			KmerType kmerType = KmerType::UNTRUSTED;
			for (size_t i = 0; i < ranking.size(); ++i) {
				ErrorType bestError = ranking[i].first;
				std::string correctedKmer = kmerAfterError(kmerLeft, bestError, kmerLeft.size() - 1);

				kmerType = kmerClassifier.classifyKmer(correctedKmer);
				int j = std::max(0, (int) multidelPos - (int) kmerClassifier.getMinKmerSize());
				while (kmerType == KmerType::REPEAT && j - 2 >= 0) {
					if (corr.correctedRead.sequence[j - 1] == '_' || corr.correctedRead.sequence[j - 2] == '_') {
						throw std::runtime_error("This should not happen");
					}
					correctedKmer = corr.correctedRead.sequence[j - 1] + correctedKmer;
					correctedKmer = corr.correctedRead.sequence[j - 2] + correctedKmer;
					kmerType = kmerClassifier.classifyKmer(correctedKmer);
					j -= 2;
				}

				if (kmerType == KmerType::TRUSTED) {
					corr.applyCorrection(bestError, multidelPos - 1, ranking[i].second);
					multidelPos++; // because we added a base left to the multidel
					break;
				}
			}
			if (kmerType == KmerType::UNTRUSTED) {
				canExtendLeft = false;
			}
			if (canExtendRight) {
				doLeft = false;
			}
		} else {
			std::string kmerRight = corr.correctedRead.sequence.substr(multidelPos + 1,
					kmerClassifier.getMinKmerSize());
			if (kmerRight.find("_") != std::string::npos) {
				kmerRight = kmerRight.substr(0, kmerRight.find("_"));
			}
			auto probs = errorProfile.getErrorProbabilities(corr.correctedRead, multidelPos + 1);
			// sort the possible corrections based on their probability
			std::vector<std::pair<ErrorType, double> > ranking;
			for (auto kv : probs) {
				if (kv.first == ErrorType::DEL_OF_A || kv.first == ErrorType::DEL_OF_C
						|| kv.first == ErrorType::DEL_OF_G || kv.first == ErrorType::DEL_OF_T) {
					ranking.push_back(kv);
				}
			}
			// sort ranking by descending kv.second
			std::sort(ranking.begin(), ranking.end(),
					[](const std::pair<ErrorType,double> &left, const std::pair<ErrorType,double> &right) {
						return left.second > right.second;
					});
			KmerType kmerType = KmerType::UNTRUSTED;
			for (size_t i = 0; i < ranking.size(); ++i) {
				ErrorType bestError = ranking[i].first;
				std::string correctedKmer = kmerAfterError(kmerRight, bestError, -1);

				kmerType = kmerClassifier.classifyKmer(correctedKmer);
				int j = multidelPos + kmerClassifier.getMinKmerSize();
				while (kmerType == KmerType::REPEAT && j + 2 < (int) corr.correctedRead.sequence.size()) {
					if (corr.correctedRead.sequence[j + 1] == '_' || corr.correctedRead.sequence[j + 2] == '_') {
						throw std::runtime_error("This should not happen");
					}
					correctedKmer = correctedKmer + corr.correctedRead.sequence[j + 1];
					correctedKmer = correctedKmer + corr.correctedRead.sequence[j + 2];
					kmerType = kmerClassifier.classifyKmer(correctedKmer);
					j += 2;
				}

				if (kmerType == KmerType::TRUSTED) {
					corr.applyCorrection(bestError, multidelPos, ranking[i].second);
					break;
				}
			}
			if (kmerType == KmerType::UNTRUSTED) {
				canExtendRight = false;
			}
			if (canExtendLeft) {
				doLeft = true;
			}
		}
		typeMiddle = classifyMiddleWithoutPos(multidelPos, corr.correctedRead.sequence, kmerClassifier);
	}

	if (typeMiddle == KmerType::UNTRUSTED) {
		return false;
	} else {
		corr.applyCorrection(ErrorType::INSERTION, multidelPos);
		return true;
	}
}

CorrectedRead postcorrectRead_Multidel(CorrectedRead &corr, ErrorProfileUnit &errorProfile,
		KmerClassificationUnit &kmerClassifier) {
	int i = 1;
	while (i < (int) corr.correctedRead.sequence.size()) {
		if (corr.correctedRead.sequence[i] == '_') { // encountered a multideletion... trying to fix it.
			std::string before = corr.correctedRead.sequence;
			bool success = resolveMultidel(corr, errorProfile, kmerClassifier, i);
			std::string after = corr.correctedRead.sequence;
			if (!success) {
				std::cout << "Could not completely resolve the deletion of multiple bases.\n";
				// TODO: Split the read :-( ... or maybe we should just leave it as it is? The information might be helpful for an assembler.
			} else {
				std::cout << "Successfully resolved a deletion of multiple bases! :-)\n";
			}
			std::cout << "Before: " << before << "\n";
			std::cout << "After : " << after << "\n";
		}
		i++;
	}
	return corr;
}

// TODO: FIXME: Improve handling of multideletions here
bool correctKmer(const std::string &kmer, size_t kmerStartPos, CorrectedRead &corr, ErrorProfileUnit &errorProfile,
		KmerClassificationUnit &kmerClassifier, bool withMultidel, bool correctIndels) {
	std::vector<std::unordered_map<ErrorType, double> > probs = errorProfile.getReadErrorProbabilitiesPartial(
			corr.correctedRead, kmerStartPos, kmerStartPos + kmer.size() - 1);
	assert(probs.size() == kmer.size());
	bool foundNewError = false;

	// sort the possible corrections based on their probability
	std::vector<std::vector<std::pair<ErrorType, double> > > probRankings;
	for (size_t i = 0; i < kmer.size(); ++i) {
		std::vector<std::pair<ErrorType, double> > rankings;
		for (auto kv : probs[i]) {
			if (!correctIndels
					&& (kv.first == ErrorType::MULTIDEL || kv.first == ErrorType::DEL_OF_A
							|| kv.first == ErrorType::DEL_OF_C || kv.first == ErrorType::DEL_OF_G
							|| kv.first == ErrorType::DEL_OF_T || kv.first == ErrorType::INSERTION))
				continue;
			rankings.push_back(kv);
		}
		// sort rankings by descending kv.second
		std::sort(rankings.begin(), rankings.end(),
				[](const std::pair<ErrorType,double> &left, const std::pair<ErrorType,double> &right) {
					return left.second > right.second;
				});
		probRankings.push_back(rankings);
	}

	for (size_t i = 0; i < kmer.size(); ++i) {
		for (size_t j = 0; j < probRankings[i].size(); ++j) {
			ErrorType bestError = probRankings[i][j].first;
			if (bestError == ErrorType::MULTIDEL)
				continue;

			if (bestError == ErrorType::SUB_FROM_A && kmer[i] == 'A')
				continue;
			if (bestError == ErrorType::SUB_FROM_C && kmer[i] == 'C')
				continue;
			if (bestError == ErrorType::SUB_FROM_G && kmer[i] == 'G')
				continue;
			if (bestError == ErrorType::SUB_FROM_T && kmer[i] == 'T')
				continue;

			//if (bestError == ErrorType::INSERTION && i == 0) continue;
			//if (bestError == ErrorType::INSERTION && i == kmer.size() - 1) continue;

			if (bestError == ErrorType::CORRECT || bestError == ErrorType::NODEL)
				continue;

			if (bestError == ErrorType::MULTIDEL || bestError == ErrorType::DEL_OF_A || bestError == ErrorType::DEL_OF_C
					|| bestError == ErrorType::DEL_OF_G || bestError == ErrorType::DEL_OF_T) {
				if (bestError == ErrorType::MULTIDEL && !withMultidel) {
					continue; // ignore Multidels for now...
				}
				if (kmerStartPos + i == corr.correctedRead.sequence.size() - 1) { // no deletion after the last base of a read
					continue;
				}

				KmerType typeLeft = checkLeftOf(kmerStartPos + i + 1, corr, kmerClassifier);
				KmerType typeRight = checkRightOf(kmerStartPos + i, corr, kmerClassifier);
				if (typeLeft != KmerType::UNTRUSTED && typeRight != KmerType::UNTRUSTED) {
					KmerType typeMiddle = classifyMiddleWithoutPos(kmerStartPos + i, corr.correctedRead.sequence,
							kmerClassifier);
					if (typeMiddle == KmerType::UNTRUSTED) {
						if (bestError == ErrorType::MULTIDEL) {
							//std::string kmerLeft = corr.correctedRead.sequence.substr(i + kmerStartPos - kmerClassifier.getMinKmerSize(), kmerClassifier.getMinKmerSize());

							corr.applyCorrection(ErrorType::MULTIDEL, i + kmerStartPos, probRankings[i][j].second);

							//std::cout << "Added multidel with kmerLeft = " << kmerLeft << "\n";

							foundNewError = true;
							return foundNewError;
						} else { // single-base deletion error
							if (bestError == ErrorType::DEL_OF_A) {
								KmerType actType = classifyMiddleWithPosAs(kmerStartPos + i,
										corr.correctedRead.sequence, kmerClassifier, "A");
								if (actType == KmerType::TRUSTED) {
									corr.applyCorrection(bestError, i + kmerStartPos, probRankings[i][j].second);
									foundNewError = true;
									return foundNewError;
								}
							} else if (bestError == ErrorType::DEL_OF_C) {
								KmerType actType = classifyMiddleWithPosAs(kmerStartPos + i,
										corr.correctedRead.sequence, kmerClassifier, "C");
								if (actType == KmerType::TRUSTED) {
									corr.applyCorrection(bestError, i + kmerStartPos, probRankings[i][j].second);
									foundNewError = true;
									return foundNewError;
								}
							} else if (bestError == ErrorType::DEL_OF_G) {
								KmerType actType = classifyMiddleWithPosAs(kmerStartPos + i,
										corr.correctedRead.sequence, kmerClassifier, "G");
								if (actType == KmerType::TRUSTED) {
									corr.applyCorrection(bestError, i + kmerStartPos, probRankings[i][j].second);
									foundNewError = true;
									return foundNewError;
								}
							} else {
								KmerType actType = classifyMiddleWithPosAs(kmerStartPos + i,
										corr.correctedRead.sequence, kmerClassifier, "T");
								if (actType == KmerType::TRUSTED) {
									corr.applyCorrection(bestError, i + kmerStartPos, probRankings[i][j].second);
									foundNewError = true;
									return foundNewError;
								}
							}
						}
					}
				}
			} else {
				if (withMultidel)
					continue;
				std::string correctedKmer = kmerAfterError(kmer, bestError, i);
				KmerType type = kmerClassifier.classifyKmer(correctedKmer);

				// extend the k-mer if it is repetitive now
				size_t inc = 0;
				while (type == KmerType::REPEAT
						&& kmerStartPos + correctedKmer.size() + inc + 1 < corr.correctedRead.sequence.size()) {
					correctedKmer += corr.correctedRead.sequence[kmerStartPos + correctedKmer.size() + inc];
					correctedKmer += corr.correctedRead.sequence[kmerStartPos + correctedKmer.size() + inc];
					type = kmerClassifier.classifyKmer(correctedKmer);
					inc += 2;
				}

				// check if the k-mer is fine now
				if (type == KmerType::TRUSTED) {
					// found the correction. Apply the correction to the corrected read,
					corr.applyCorrection(bestError, i + kmerStartPos, probRankings[i][j].second);
					foundNewError = true;
					return foundNewError;
				}
			}
		}
	}
	return foundNewError;
}

// TODO FIXME: Improve k-mer covering of the read...
bool precorrectRead_KmerBased(CorrectedRead &corr, ErrorProfileUnit &errorProfile,
		KmerClassificationUnit &kmerClassifier, bool withMultidel, bool correctIndels) {
	// cover the read with k-mers and correct them
	bool foundNewError = false;
	size_t i = 0;
	while (i < corr.correctedRead.sequence.size()) {
		std::string kmerString = corr.correctedRead.sequence.substr(i, kmerClassifier.getMinKmerSize());
		if (kmerString.find("_") != std::string::npos) { // kmer contains multidel, thus it should be ignored here
			i++;
			continue;
		}
		KmerType kmerType = kmerClassifier.classifyKmer(kmerString);
		size_t j = 0;
		while (kmerType == KmerType::REPEAT && i + kmerString.size() + j + 1 < corr.correctedRead.sequence.size()) {
			if (corr.correctedRead.sequence[i + kmerString.size() + j] == '_'
					|| corr.correctedRead.sequence[i + kmerString.size() + j + 1] == '_') {
				break;
			}
			kmerString += corr.correctedRead.sequence[i + kmerString.size() + j];
			kmerString += corr.correctedRead.sequence[i + kmerString.size() + j];
			kmerType = kmerClassifier.classifyKmer(kmerString);
		}
		if (kmerType == KmerType::UNTRUSTED) {
			foundNewError |= correctKmer(kmerString, i, corr, errorProfile, kmerClassifier, withMultidel,
					correctIndels);
		}

		//i += std::max(1, (int) kmerString.size() - 1);

		if (correctIndels) {
			i += std::max(1, (int) kmerString.size() / 2);
		} else {
			i += kmerString.size();
		}
	}

	return foundNewError;
}

CorrectedRead precorrectRead_Naive(CorrectedRead &corr, ErrorProfileUnit &errorProfile, bool correctIndels) {
	int i = 0;
	while (i < (int) corr.correctedRead.sequence.size()) {
		auto probs = errorProfile.getErrorProbabilities(corr.correctedRead, i);
		std::pair<ErrorType, double> bestCurrent = mostLikelyCurrentBase(probs);
		if (bestCurrent.first != ErrorType::CORRECT) {
			if (!correctIndels
					&& (bestCurrent.first == ErrorType::MULTIDEL || bestCurrent.first == ErrorType::DEL_OF_A
							|| bestCurrent.first == ErrorType::DEL_OF_C || bestCurrent.first == ErrorType::DEL_OF_G
							|| bestCurrent.first == ErrorType::DEL_OF_T || bestCurrent.first == ErrorType::INSERTION)) {
				continue;
			}

			if (!((corr.correctedRead.sequence[i] == 'A' && bestCurrent.first == ErrorType::SUB_FROM_A)
					|| (corr.correctedRead.sequence[i] == 'C' && bestCurrent.first == ErrorType::SUB_FROM_C)
					|| (corr.correctedRead.sequence[i] == 'G' && bestCurrent.first == ErrorType::SUB_FROM_G)
					|| (corr.correctedRead.sequence[i] == 'T' && bestCurrent.first == ErrorType::SUB_FROM_T))) {

				corr.applyCorrection(bestCurrent.first, i, bestCurrent.second);
				if (bestCurrent.first == ErrorType::INSERTION) {
					assert(i > 0); // there should be no insertion at the beginning of a read
					i--;
				}
			}
		}

		probs = errorProfile.getErrorProbabilities(corr.correctedRead, i);
		std::pair<ErrorType, double> bestNext = mostLikelyNextGap(probs);
		if (bestNext.first != ErrorType::NODEL) {
			corr.applyCorrection(bestNext.first, i, bestNext.second);
		}

		++i;
	}

	return corr;
}

CorrectedRead correctRead_KmerBased(const FASTQRead &fastqRead, ErrorProfileUnit &errorProfile,
		KmerClassificationUnit &kmerClassifier, bool correctIndels) {
	CorrectedRead corr(fastqRead);
	//bool foundNewError =
	precorrectRead_KmerBased(corr, errorProfile, kmerClassifier, false, correctIndels);
	//int maxIterations = 100;
	//int actIteration = 0;
	//std::unordered_set<std::string> previousCorrections;
	//previousCorrections.insert(corr.correctedRead.sequence);

	/*if (!foundNewError) {
	 std::cout << "The read was correct. :)\n";
	 }*/

	/*while (foundNewError && actIteration < maxIterations) {
	 //std::cout << "new iteration\n";
	 foundNewError = precorrectRead_KmerBased(corr, errorProfile, kmerClassifier, false, correctIndels);
	 if (!foundNewError) break;

	 std::string seqNow = corr.correctedRead.sequence;
	 if (previousCorrections.find(seqNow) != previousCorrections.end()) {
	 //std::cout << "Detected a previously encountered correction after " << actIteration + 1 << " iterations. -> correction terrace? \n";
	 break;
	 } else {
	 previousCorrections.insert(seqNow);
	 }
	 actIteration++;
	 if (actIteration == maxIterations) {
	 std::cout << "Stuck in endless loop?\n";
	 }
	 }*/
	//std::cout << "did " << actIteration+1 << " iterations.\n";
	//precorrectRead_KmerBased(corr, errorProfile, kmerClassifier, true);
	//postcorrectRead_Multidel(corr, errorProfile, kmerClassifier);
	return corr;
}

CorrectedRead correctRead_Naive(const FASTQRead &fastqRead, ErrorProfileUnit &errorProfile,
		KmerClassificationUnit &kmerClassifier, bool correctIndels) {
	CorrectedRead corr(fastqRead);
	precorrectRead_Naive(corr, errorProfile, correctIndels);
	//return postcorrectRead_Multidel(corr, errorProfile, kmerClassifier);
	return corr;
}

