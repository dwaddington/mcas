import React, { useContext } from 'react';
import cx from 'classnames';
import NavContext from 'gatsby-theme-carbon/src/util/context/NavContext';
import { nav, open, divider, link, linkDisabled } from 'gatsby-theme-carbon/src/components/Switcher/Switcher.module.scss';

const Switcher = ({ children }) => {
  const { switcherIsOpen } = useContext(NavContext);

  return (
    <nav
      className={cx(nav, { [open]: switcherIsOpen })}
      aria-label="IBM Design ecosystem navigation"
      aria-expanded={switcherIsOpen}
      tabIndex="-1"
    >
      <ul>{children}</ul>
    </nav>
  );
};

export const SwitcherDivider = (props) => (
  <li className={divider}>
    <span {...props} />
  </li>
);

export const SwitcherLink = ({
  disabled,
  children,
  href: hrefProp,
  ...rest
}) => {
  const href = disabled || !hrefProp ? undefined : hrefProp;
  const className = disabled ? linkDisabled : link;
  const { switcherIsOpen } = useContext(NavContext);

  return (
    <li>
      <a
        aria-disabled={disabled}
        role="button"
        tabIndex={switcherIsOpen ? 0 : '-1'}
        className={className}
        href={href}
        {...rest}
      >
        {children}
      </a>
    </li>
  );
};

// https://css-tricks.com/using-css-transitions-auto-dimensions/
// Note: if you change this, update the max-height in the switcher stylesheet
const DefaultChildren = () => {
  return (
    <>
      <SwitcherLink href="https://ibm.com/design">IBM Foobar Design</SwitcherLink>
      <SwitcherLink href="https://ibm.com/design/language">
        IBM Design Language
      </SwitcherLink>
      <SwitcherLink href="https://ibm.com/brand">IBM Funky Brand Center</SwitcherLink>
      <SwitcherDivider>Design disciplines</SwitcherDivider>
      <SwitcherLink href="https://www.carbondesignsystem.com/">
        Product
      </SwitcherLink>
      <SwitcherLink href="https://www.ibm.com/standards/web/">
        Digital
      </SwitcherLink>
      <SwitcherLink disabled>Disabled Link</SwitcherLink>
      <SwitcherDivider>About</SwitcherDivider>
      <SwitcherLink href="https://www.research.ibm.com/">
        IBM Research Division
      </SwitcherLink>
      <SwitcherLink href="https://www.ibm.com/it-infrastructure/storage/software-defined-storage">
          IBM Software Defined Storage
      </SwitcherLink>    
    </>
  );
};

Switcher.defaultProps = {
  children: <DefaultChildren />,
};

export default Switcher;
