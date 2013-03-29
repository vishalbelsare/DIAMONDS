#include "Results.h"


// Results::Results()
//
// PURPOSE: 
//      Constructor. Sets nested sampler object and output directory path.
//
// INPUT:
//      nestedSampler: a NestedSampler class object used as the container of
//      information to write from.
//

Results::Results(NestedSampler &nestedSampler)
: nestedSampler(nestedSampler)
{
} 










// Results::~Results()
//
// PURPOSE: 
//      Destructor.
//

Results::~Results()
{

}












// Results::posteriorProbability()
//
// PURPOSE:
//      Saves the posterior probability for the sample obtained from the 
//      nested sampling into a one dimensional Eigen Array. 
//
// OUTPUT:
//      An Eigen Array containing the values of the posterior probability
//      sorted according to the nesting algorithm process.
// 
// REMARK:
//      Values are real probabilities (and not probability densities).
//

ArrayXd Results::posteriorProbability()
{
    ArrayXd logPosteriorDistribution = nestedSampler.logWeightOfPosteriorSample - nestedSampler.getLogEvidence();
    ArrayXd posteriorDistribution = logPosteriorDistribution.exp();
    
    return posteriorDistribution;
}









// Results:writeParameterEstimationToFile()
//
// PURPOSE:
//      computes the expectation, median and mode values from the 
//      marginalized posterior probability. 
//      Shortest Bayesian credible intervals (CI) are also computed.
//      All the values are stored in to a bidimensional nEigen Array.
//
// INPUT:
//      credibleLevel: a double number providing the desired credible 
//      level to be computed. Default value corresponds to most 
//      used credible level of 68.27 %.
//      
// OUTPUT:
//      A bidimensional Eigen Array containing all the estimates of the
//      free parameters (one parameter per row).
// 

ArrayXXd Results::parameterEstimation(const double credibleLevel)
{
    int Ndimensions = nestedSampler.posteriorSample.rows();
    int Niterations = nestedSampler.posteriorSample.cols();
    ArrayXd parameterComponent;
    ArrayXd marginalDistribution;
    ArrayXd posteriorDistribution = posteriorProbability();
    ArrayXXd parameterEstimates;


    parameterEstimates.resize(Ndimensions, 5);


    // Loop over all free parameters

    for (int i = 0; i < Ndimensions; i++)
    {
        parameterComponent = nestedSampler.posteriorSample.row(i);
        marginalDistribution = posteriorDistribution;


        // Sort elements of array parameterComponent in increasing
        // order and sort elements of array marginalDistribution accordingly
        
        Functions::sortElements(parameterComponent, marginalDistribution); 
        

        // Marginalize over parameter by merging posterior values 
        // corresponding to equal parameter values

        int NduplicateParameterComponents = 0;

        for (int j = 0; j < Niterations - 1; j++)
        {  
            if (parameterComponent(j) == -DBL_MAX)
                continue;
            else
            {
                for (int k = j + 1; k < Niterations; k++)
                {
                    if (parameterComponent(k) == -DBL_MAX)
                        continue;
                    else
                        if (parameterComponent(j) == parameterComponent(k))
                        {   
                            parameterComponent(k) = -DBL_MAX;        // Set duplicate to bad value (flag)
                            marginalDistribution(j) = marginalDistribution(j) + marginalDistribution(k); // Merge probability values
                            marginalDistribution(k) = 0.0;
                            NduplicateParameterComponents++;
                        }
                }
            }
        }


        // Remove bad points and store final values in array copies

        if (NduplicateParameterComponents > 0) // Check if bad points are present otherwise skip block
        {
            ArrayXd parameterComponentCopy(Niterations - NduplicateParameterComponents);
            ArrayXd marginalDistributionCopy(Niterations - NduplicateParameterComponents);
        
            int n = 0;

            for (int m = 0; (m < Niterations) && (n < (Niterations - NduplicateParameterComponents)); m++)
            {
                if (parameterComponent(m) == -DBL_MAX)
                    continue;
                else
                    if (parameterComponent(m) != -DBL_MAX)
                        {
                            parameterComponentCopy(n) = parameterComponent(m);
                            marginalDistributionCopy(n) = marginalDistribution(m);
                            n++;
                        }
            }


            // Replace original marginal arrays with array copies

            parameterComponent = parameterComponentCopy;
            marginalDistribution = marginalDistributionCopy;
        }


        // Compute the mean value (expectation value)
       
        double meanParameter;
        
        meanParameter = (parameterComponent.cwiseProduct(marginalDistribution)).sum();
        parameterEstimates(i,0) = meanParameter;


        // Compute the median value (value corresponding to 50% of probability)

        double medianParameter;
        double totalProbability = 0.0;
        int k = 0;
        
        while (totalProbability < 0.5)
        {
            medianParameter = parameterComponent(k);
            totalProbability += marginalDistribution(k);
            k++;
        }
        
        parameterEstimates(i,1) = medianParameter;


        // Find the mode value (parameter corresponding to maximum probability value)

        int max = 0;                                    // Subscript corresponding to mode value
        double maximumMarginal;
        double maximumParameter;

        maximumMarginal = marginalDistribution.maxCoeff(&max);
        maximumParameter = parameterComponent(max);
        parameterEstimates(i,2) = maximumParameter;

        
        // Compute the "shortest" credible intervals (CI)

        int stepRight = 1;
        double limitProbabilityRight;
        double limitProbabilityLeft;
        double limitParameterLeft;
        double limitParameterRight;
        ArrayXd marginalDistributionLeft(max + 1);       // Marginal distribution up to mode value
        ArrayXd parameterComponentLeft(max + 1);          // Parameter range up to mode value
        ArrayXd marginalDifferenceLeft;                  // Difference distribution to find point in 
                                                         // in the left-hand distribution having equal
                                                         // probability to that identified in the right-hand part
        for (int nn = 0; nn <= max; nn++)
        {
            marginalDistributionLeft(nn) = marginalDistribution(nn);          // Consider left-hand distribution (up to mode value)
            parameterComponentLeft(nn) = parameterComponent(nn);
        }


        while (totalProbability < (credibleLevel/100.))     // Stop when probability >= credibleLevel 
        {
            totalProbability = 0.0;
            limitProbabilityRight = marginalDistribution(max + stepRight);
            limitParameterRight = parameterComponent(max + stepRight);
            marginalDifferenceLeft = (marginalDistributionLeft - limitProbabilityRight).abs();

            int min = 0;

            limitProbabilityLeft = marginalDifferenceLeft.minCoeff(&min);
            limitProbabilityLeft = marginalDistribution(min);
            limitParameterLeft = parameterComponent(min);

            for (int t = min; t <= (max + stepRight); t++)
            {
                totalProbability += marginalDistribution(t);        // Evaluate total probability within the range
            }

            stepRight++;
        }

        double lowerCredibleInterval;
        double upperCredibleInterval;

        lowerCredibleInterval = maximumParameter - limitParameterLeft;
        upperCredibleInterval = limitParameterRight - maximumParameter;
           
        parameterEstimates(i,3) = lowerCredibleInterval;
        parameterEstimates(i,4) = upperCredibleInterval;

    }

    return parameterEstimates;
}













// Results::writeParametersToFile()
//
// PURPOSE: 
//      writes the parameters values from the nested sampling
//      in separate ASCII files having one column format each.
//      
// OUTPUT:
//      void
// 

void Results::writeParametersToFile(string pathPrefix, string outputFileExtension)
{
    File::arrayXXdRowsToFiles(nestedSampler.posteriorSample, pathPrefix, outputFileExtension);
}













// Results::writeLogLikelihoodToFile()
//
// PURPOSE:
//      writes the log likelihood values from the nested sampling
//      into an ASCII file of one column format. The values are
//      sorted in increasing order.
//
// OUTPUT:
//      void
// 

void Results::writeLogLikelihoodToFile(string fullPath)
{
    ofstream outputFile;
    File::openOutputFile(outputFile, fullPath);
            
    outputFile << "# Posterior sample from nested sampling" << endl;
    outputFile << "# log(Likelihood)" << endl;
    outputFile << scientific << setprecision(9);
    File::arrayXdToFile(outputFile, nestedSampler.logLikelihoodOfPosteriorSample);
    outputFile.close();

}











// Results::writeEvidenceInformationToFile()
//
// PURPOSE:
//      writes the evidence from the nested sampling into an ASCII file. 
//      Evidence error and information Gain are also included.
//
// OUTPUT:
//      void
//

void Results::writeEvidenceInformationToFile(string fullPath)
{
    ofstream outputFile;
    File::openOutputFile(outputFile, fullPath);
            
    outputFile << "# Evidence results from nested sampling" << endl;
    outputFile << "# log(Evidence)" << setw(12) << "Error of log(Evidence)" << setw(12) << "Information Gain" << endl;
    outputFile << scientific << setprecision(9);
    outputFile << nestedSampler.getLogEvidence() << setw(12);
    outputFile << nestedSampler.getLogEvidenceError() << setw(12);
    outputFile << nestedSampler.getInformationGain() << endl;
    outputFile.close();

} 












// Results::writePosteriorProbabilityToFile()
//
// PURPOSE:
//      writes the posterior probability for the sample obtained from the 
//      nested sampling into an ASCII file of one column format.
//
// OUTPUT:
//      void
// 

void Results::writePosteriorProbabilityToFile(string fullPath)
{
    ArrayXd posteriorDistribution = posteriorProbability();

    ofstream outputFile;
    File::openOutputFile(outputFile, fullPath);
            
    outputFile << "# Posterior probability distribution from nested sampling" << endl;
    outputFile << scientific << setprecision(9);
    File::arrayXdToFile(outputFile, posteriorDistribution);
    outputFile.close();

} 









// Results:writeParameterEstimationToFile()
//
// PURPOSE:
//      Writes the expectation, median and mode values from the 
//      marginalized posterior probability into an ASCII file. 
//      Shortest Bayesian credible intervals (CI) are also included.
//
// INPUT:
//      credibleLevel: a double number providing the desired credible 
//      level to be computed. Default value correspond to most 
//      used credible level of 68.27 %.
//      
// OUTPUT:
//
// 

void Results::writeParameterEstimationToFile(string fullPath, const double credibleLevel)
{
    ArrayXXd parameterEstimates = parameterEstimation(credibleLevel);


    // Write output ASCII file

    ofstream outputFile;
    File::openOutputFile(outputFile, fullPath);
            
    outputFile << "# Summary of Parameter Estimation from nested sampling" << endl;
    outputFile << "# Credible intervals are the shortest credible intervals" << endl; 
    outputFile << "# according to the usual definition" << endl;
    outputFile << "# Credible level: " << fixed << setprecision(2) << credibleLevel << " %" << endl;
    outputFile << "# Column #1: Expectation" << endl;
    outputFile << "# Column #2: Median" << endl;
    outputFile << "# Column #3: Mode" << endl;
    outputFile << "# Column #4: Lower Credible Interval (CI)" << endl;
    outputFile << "# Column #5: Upper Credible Interval (CI)" << endl;
    outputFile << scientific << setprecision(9);
    File::arrayXXdToFile(outputFile, parameterEstimates);
    outputFile.close();

} 


