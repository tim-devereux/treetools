// Copyright (c) 2022
// Commonwealth Scientific and Industrial Research Organisation (CSIRO)
// ABN 41 687 119 230
//
// Author: Thomas Lowe
#include "treelib/treeutils.h"
#include "treelib/treepruner.h"
#include <raylib/rayparse.h>
#include <raylib/raycloud.h>
#include <raylib/rayrenderer.h>
#include "raylib/raytreegen.h"
#include <cstdlib>
#include <iostream>

void usage(int exit_code = 1)
{
  std::cout << "Placeholder method to grow or shrink the tree from the tips, using a linear model. No branches are added." << std::endl;
  std::cout << "usage:" << std::endl;
  std::cout << "treegrow forest.txt -1 years    - reduce the tree according to the rates, default values below." << std::endl;
  std::cout << "                    --length_rate 0.3   - expected branch length increase per year in m" << std::endl;
  std::cout << "                    --width_rate  0.004 - expected branch diameter increase per year in m" << std::endl;
  exit(exit_code);
}

// Read in a ray cloud and convert it into an array for topological optimisation
int main(int argc, char *argv[])
{
  ray::FileArgument forest_file;
  ray::DoubleArgument period(-1000, 3), length_rate(0.0001, 1000.0), width_rate(0.001, 10.0);
  ray::TextArgument years("years");
  ray::OptionalKeyValueArgument length_option("length_rate", 'l', &length_rate);
  ray::OptionalKeyValueArgument width_option("width_rate", 'w', &width_rate);

  bool parsed = ray::parseCommandLine(argc, argv, {&forest_file, &period, &years}, {&length_option, &width_option});
  if (!parsed)
  {
    usage();
  }

  ray::ForestStructure forest;
  if (!forest.load(forest_file.name()))
  {
    usage();
  }  
  if (forest.trees.size() != 0 && forest.trees[0].segments().size() == 0)
  {
    std::cout << "grow only works on tree structures, not trunks-only files" << std::endl;
    usage();
  }
  double len_rate = length_option.isSet() ? length_rate.value() : 0.3;
  double diam_rate = width_option.isSet() ? width_rate.value() : 0.004;
  double radius_growth = 0.5 * diam_rate * period.value();
  double length_growth = len_rate * period.value();
  for (auto &tree: forest.trees)
  {
    for (auto &segment: tree.segments())
    {
      segment.radius = segment.radius + radius_growth;
    }
  }

  ray::ForestStructure grown_forest;
  if (period.value() > 0.0)
  {
    grown_forest = forest;
    // Just extend the leaves linearly
    for (size_t t = 0; t<forest.trees.size(); t++)
    {
      auto &tree = forest.trees[t];
      auto &new_tree = grown_forest.trees[t];
      std::vector< std::vector<int> > children(tree.segments().size());
      std::vector<double> min_length_from_leaf(tree.segments().size(), 1e10);
      for (size_t i = 1; i<tree.segments().size(); i++)
        children[tree.segments()[i].parent_id].push_back((int)i);
      for (size_t i = 0; i<tree.segments().size(); i++)
      {
        if (children[i].size() == 0) // a leaf, so ...
        {
          ray::TreeStructure::Segment segment = tree.segments()[i];
          segment.radius -= radius_growth;
          int par = segment.parent_id;
          Eigen::Vector3d dir(0,0,1);
          if (par != -1)
            dir = (tree.segments()[i].tip - tree.segments()[par].tip).normalized();
          segment.tip += dir * length_growth;
          segment.parent_id = (int)i;
          new_tree.segments().push_back(segment);
        }
      }    
    }
  }
  else
  {
    ray::ForestStructure pruned_forest;
    tree::pruneLength(forest, -length_growth, pruned_forest);
    if (pruned_forest.trees.empty())
    {
      std::cout << "Warning: no trees left after shrinking. No file saved." << std::endl;
      return 1;
    }

    const double minimum_branch_diameter = 0.001;
    tree::pruneDiameter(pruned_forest, minimum_branch_diameter, grown_forest);
  }

  if (grown_forest.trees.empty())
  {
    std::cout << "Warning: no trees left after pruning. No file saved." << std::endl;
  }
  else
  {
    grown_forest.save(forest_file.nameStub() + "_grown.txt");
  }
  return 1;
}

