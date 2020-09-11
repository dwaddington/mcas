import React from 'react';
import Switcher from 'gatsby-theme-carbon/src/components/Switcher';
import { SwitcherLink } from 'gatsby-theme-carbon/src/components/Switcher';
//import { SwitcherDivider } from 'gatsby-theme-carbon/src/components/Switcher';

const CustomSwitcher = (props) => (
  <Switcher {...props}>
    <SwitcherLink href="https://www.research.ibm.com/labs/almaden/">IBM Research Almaden</SwitcherLink> {}
    <SwitcherLink href="https://www.ibm.com/design/">IBM Design</SwitcherLink> {}
  </Switcher>
);

export default CustomSwitcher;
