/*
 * Copyright (c) 2017, Adrian Michel
 * http://www.amichel.com
 *
 * This software is released under the 3-Clause BSD License
 *
 * The complete terms can be found in the attached LICENSE file
 * or at https://opensource.org/licenses/BSD-3-Clause
 */

#pragma once

#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
#include <boost/scope_exit.hpp>
#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>

#include "individual.hpp"
#include "listener.hpp"
#include "multithread.hpp"
#include "mutation_strategy.hpp"
#include "population.hpp"
#include "processors.hpp"
#include "random_generator.hpp"
#include "selection_strategy.hpp"
#include "termination_strategy.hpp"

namespace amichel {
namespace de {

/**
 * Exception thrown in case of an error during an optimization
 * session
 *
 * @author adrian (12/1/2011)
 */
class differential_evolution_exception {};

/**
 * Differential evolution main class
 *
 * Runs an optimization session based on various input
 * parameters or strategies
 *
 * @author adrian (12/1/2011)
 */
template <typename T>
class differential_evolution {
 private:
  const size_t m_varCount;
  const size_t m_popSize;

  population_ptr m_pop1;
  population_ptr m_pop2;
  individual_ptr m_bestInd;

  constraints_ptr m_constraints;
  typename processors<T>::processors_ptr m_processors;
  termination_strategy& m_terminationStrategy;
  selection_strategy_ptr m_selectionStrategy;
  mutation_strategy_ptr m_mutationStrategy;
  listener& m_listener;

  const bool m_minimize;

 public:
  /**
   * constructs a differential_evolution object
   *
   * @author adrian (12/4/2011)
   *
   * @param varCount total number of variables. It includes the
   *  			   variables required by the objective function
   *  			   but has many more elements as required by the
   *  			   algorithm
   * @param popSize total number of individuals in a population
   * @param processors number of parallel processors used
   *  				 during an optimization session
   * @param constraints a vector of constraints that contains the
   *  				  constraints for the variables used by the
   *  				  objective function as well as constraints
   *  				  for all other variables used internally by
   *  				  the algorithm
   * @param minimize will attempt to minimize the cost if true, or
   *  			   maximize the cost if false
   * @param terminationStrategy a termination strategy
   * @param selectionStrategy a selection strategy
   * @param mutationStrategy a mutation strategy
   * @param listener a listener
   */
  differential_evolution(size_t varCount, size_t popSize,
                         typename processors<T>::processors_ptr processors,
                         constraints_ptr constraints, bool minimize,
                         termination_strategy& terminationStrategy,
                         selection_strategy_ptr selectionStrategy,
                         mutation_strategy_ptr mutationStrategy,
                         listener& listener) try

      : m_varCount(varCount),
        m_popSize(popSize),
        m_pop1(boost::make_shared<population>(popSize, varCount, constraints)),
        m_pop2(boost::make_shared<population>(popSize, varCount)),
        m_bestInd(m_pop1->best(minimize)),
        m_constraints(constraints),
        m_processors(processors),
        m_terminationStrategy(terminationStrategy),
        m_selectionStrategy(selectionStrategy),
        m_mutationStrategy(mutationStrategy),
        m_listener(listener),
        m_minimize(minimize) {
    assert(processors);
    assert(constraints);
    assert(selectionStrategy);
    assert(mutationStrategy);

    assert(popSize > 0);
    assert(varCount > 0);

    // initializing population 1 by running all objective functions with
    // the initial random arguments
    processors->push(m_pop1);
    processors->start();
    processors->wait();

  } catch (const processors_exception&) {
    throw differential_evolution_exception();
  }

  virtual ~differential_evolution(void) {}

  /**
   * starts a differential evolution optimization process
   *
   * although the processing is done in parallel, this function is
   * synchronous and won't return until the optimization is
   * complete, or an error triggered an exception
   *
   * @author adrian (12/4/2011)
   */
    void run()
    {
        try
        {
            m_listener.start();
            individual_ptr bestIndIteration(m_bestInd);
            size_t genCount;

            for (genCount = 0; m_terminationStrategy.check_termination(m_bestInd, genCount); ++genCount)
            {
                m_listener.startGeneration(genCount);
                for (size_t i = 0; i < m_popSize; ++i)
                {
                    mutation_strategy::mutation_info mutationInfo(
                        (*m_mutationStrategy)(*m_pop1, bestIndIteration, i));

                    individual_ptr tmpInd(boost::tuples::get<0>(mutationInfo));

                    tmpInd->ensureConstraints(m_constraints,
                    boost::tuples::get<1>(mutationInfo));

                    // populate the queue
                    m_processors->push(tmpInd);

                    // put temps in a temp vector for now (they are empty until
                    // processed), will be moved to the right place after processed
                    (*m_pop2)[i] = tmpInd;
                }

                m_listener.startProcessors(genCount);
                m_processors->start();
                m_processors->wait();
                m_listener.endProcessors(genCount);

                // BestParentChildSelectionStrategy()( m_pop1, m_pop2, m_bestInd,
                // m_minimize );
                m_listener.startSelection(genCount);
                (*m_selectionStrategy)(m_pop1, m_pop2, m_bestInd, m_minimize);
                bestIndIteration = m_bestInd;

                m_listener.endSelection(genCount);

                m_listener.endGeneration(genCount, bestIndIteration, m_bestInd);
            }

            m_listener.end(genCount, m_bestInd);
        }
        catch (const processors_exception&)
        {
            m_listener.error();
            throw differential_evolution_exception();
        }
    }

  /**
   * returns the best individual resulted from the optimization
   * process
   *
   * @author adrian (12/4/2011)
   *
   * @return individual_ptr
   */
  individual_ptr best() const { return m_bestInd; }
};
}  // namespace de
}  // namespace amichel
