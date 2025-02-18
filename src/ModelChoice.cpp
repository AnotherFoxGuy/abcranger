#include <fmt/format.h>
#include "ModelChoice.hpp"
#include "ForestOnlineClassification.hpp"
#include "ForestOnlineRegression.hpp"
#include "matutils.hpp"
#include "various.hpp"

#include "DataDense.h"
#include "cxxopts.hpp"
#include <algorithm>
#include <fstream>
#include "range/v3/all.hpp"

using namespace ranger;
using namespace Eigen;
using namespace ranges;

ModelChoiceResults ModelChoice_fun(Reftable &myread,
                                   std::vector<double> obs,
                                   const cxxopts::ParseResult& opts,
                                   bool quiet)
{
    size_t nref, ntree, nthreads, noisecols, seed, minnodesize;
    std::string outfile;   
    bool lda,seeded;

    nref = opts["n"].as<size_t>();
    ntree = opts["t"].as<size_t>();
    nthreads = opts["j"].as<size_t>();
    noisecols = opts["c"].as<size_t>();
    seeded = opts.count("s") != 0;
    if (seeded)
        seed = opts["s"].as<size_t>();
    minnodesize = opts["m"].as<size_t>();
    lda =  opts.count("nolinear") == 0;
    outfile = (opts.count("output") == 0) ? "modelchoice_out" : opts["o"].as<std::string>();

    std::vector<double> samplefract{std::min(1e5,static_cast<double>(myread.nrec))/static_cast<double>(myread.nrec)};
    auto nstat = myread.stats_names.size();
    size_t K = myread.nrecscen.size();
    MatrixXd statobs(1, nstat);
    statobs = Map<MatrixXd>(obs.data(), 1, nstat);

    if (lda) addLda(myread, statobs);

    addNoise(myread, statobs, noisecols);

    addScen(myread);
    std::vector<string> varwithouty(myread.stats_names.size()-1);
    for(auto i = 0; i < varwithouty.size(); i++) varwithouty[i] = myread.stats_names[i];

    auto datastatobs = unique_cast<DataDense, Data>(std::make_unique<DataDense>(statobs, varwithouty, 1, varwithouty.size()));
    auto datastats = unique_cast<DataDense, Data>(std::make_unique<DataDense>(myread.stats, myread.stats_names, myread.nrec, myread.stats_names.size()));
    ForestOnlineClassification forestclass;
    forestclass.init("Y",                       // dependant variable
                     MemoryMode::MEM_DOUBLE,    // memory mode double or float
                     std::move(datastats),      // data
                     std::move(datastatobs),    // predict
                     0,                         // mtry, if 0 sqrt(m -1) but should be m/3 in regression
                     outfile,        // output file name prefix
                     ntree,                     // number of trees
                     (seeded ? seed : r()),                    // seed rd()
                     nthreads,                  // number of threads
                     ranger::IMP_GINI,          // Default IMP_NONE
                     minnodesize,                         // default min node size (classif = 1, regression 5)
                     "",                        // status variable name, only for survival
                     false,                     // prediction mode (true = predict)
                     true,                      // replace
                     std::vector<string>(0),    // unordered variables names
                     false,                     // memory_saving_splitting
                     DEFAULT_SPLITRULE,         // gini for classif variance for  regression
                     true,                     // predict_all
                     samplefract,   // sample_fraction 1 if replace else 0.632
                     DEFAULT_ALPHA,             // alpha
                     DEFAULT_MINPROP,           // miniprop
                     false,                     // holdout
                     DEFAULT_PREDICTIONTYPE,    // prediction type
                     DEFAULT_NUM_RANDOM_SPLITS, // num_random_splits
                     false,                     //order_snps
                     DEFAULT_MAXDEPTH);         // max_depth

    ModelChoiceResults res;
    if (!quiet) {
        forestclass.verbose_out = &std::cout;
        std::cout << "///////////////////////////////////////// First forest (training on ABC output)" << std::endl;
    }

    forestclass.run(!quiet, true);
    
    auto preds = forestclass.getPredictions();
    // Overall oob error
    res.oob_error = forestclass.getOverallPredictionError();
    // Confusion Matrix
    res.confusion_matrix = forestclass.getConfusion();
    if (!quiet) forestclass.writeConfusionFile();
    // Variable Importance
    res.variable_importance = forestclass.getImportance();
    if (!quiet) forestclass.writeImportanceFile();
    // OOB error by number of trees;
    res.ntree_oob_error =  preds[2][0];
    if (!quiet) forestclass.writeOOBErrorFile();

    vector<size_t> votes(K);
    for(auto& tree_pred : preds[1][0]) votes[static_cast<size_t>(tree_pred-1)]++;
    res.votes = votes;

    size_t predicted_model = std::distance(votes.begin(),std::max_element(votes.begin(),votes.end()));
    res.predicted_model = predicted_model;

    bool machin = false;
    auto dataptr = forestclass.releaseData();
    auto& datareleased = static_cast<DataDense&>(*dataptr.get());
    size_t ycol = datareleased.getNumCols() - 1;
    
    for(size_t i = 0; i < preds[0][0].size(); i++) {
        if (!std::isnan(preds[0][0][i]))
            datareleased.set(ycol,i,preds[0][0][i] == myread.scenarios[i] ? 1.0 : 0.0, machin);
    }

    std::vector<size_t> defined_preds = preds[0][0]
        | view::enumerate
        | view::filter([](auto d){ return !std::isnan(d.second); })
        | view::keys;
    datareleased.filterRows(defined_preds);

    auto statobsreleased = forestclass.releasePred();
    ForestOnlineRegression forestreg;


    forestreg.init("Y",                       // dependant variable
                     MemoryMode::MEM_DOUBLE,    // memory mode double or float
                     std::move(dataptr),    // data
                     std::move(statobsreleased),  // predict
                     0,                         // mtry, if 0 sqrt(m -1) but should be m/3 in regression
                     outfile,              // output file name prefix
                     ntree,                     // number of trees
                     (seeded ? seed : r()),                    // seed rd()
                     nthreads,                  // number of threads
                     DEFAULT_IMPORTANCE_MODE,  // Default IMP_NONE
                     5,                         // default min node size (classif = 1, regression 5)
                     "",                        // status variable name, only for survival
                     false,                     // prediction mode (true = predict)
                     true,                      // replace
                     std::vector<string>(0),        // unordered variables names
                     false,                     // memory_saving_splitting
                     DEFAULT_SPLITRULE,         // gini for classif variance for  regression
                     false,                     // predict_all
                     samplefract,   // sample_fraction 1 if replace else 0.632
                     DEFAULT_ALPHA,             // alpha
                     DEFAULT_MINPROP,           // miniprop
                     false,                     // holdout
                     DEFAULT_PREDICTIONTYPE,    // prediction type
                     DEFAULT_NUM_RANDOM_SPLITS, // num_random_splits
                     false,                     //order_snps
                     DEFAULT_MAXDEPTH);   

    if (!quiet) forestreg.verbose_out = &std::cout;
    if (!quiet) {
        forestclass.verbose_out = &std::cout;
        std::cout << "///////////////////////////////////////// Second forest (training on error)" << std::endl;
    }

    forestreg.run(!quiet,true);

    auto dataptr2 = forestreg.releaseData();
    auto& datareleased2 = static_cast<DataDense&>(*dataptr2.get());
    datareleased2.data.conservativeResize(NoChange,nstat);
    myread.stats = std::move(datareleased2.data);
    myread.stats_names.resize(nstat);

    auto predserr = forestreg.getPredictions();
    res.post_proba = predserr[1][0][0];
    const std::string& predict_filename = outfile + ".predictions";

    std::ostringstream os;
    for(auto i = 0; i < votes.size(); i++) {
        os << fmt::format(" votes model{0}",i+1);
    }
    os << fmt::format(" selected model");
    os << fmt::format(" post proba\n");
    for(auto i = 0; i < votes.size(); i++) {
        os << fmt::format("{:>13}",votes[i]);
    }
    os << fmt::format("{:>15}", predicted_model + 1);
    os << fmt::format("{:11.3f}\n",predserr[1][0][0]);
    if (!quiet) std::cout << os.str();
    std::cout.flush();

    std::ofstream predict_file;
    if (!quiet) {
        predict_file.open(predict_filename, std::ios::out);
        if (!predict_file.good()) {
            throw std::runtime_error("Could not write to prediction file: " + predict_filename + ".");
        }
        predict_file << os.str();
        predict_file.flush();
        predict_file.close();
    }


    return res;
}